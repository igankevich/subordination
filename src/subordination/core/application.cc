#include <subordination/core/application.hh>

#include <algorithm>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <utility>

#include <grp.h>
#include <stdlib.h>

#include <unistdx/base/check>
#include <unistdx/base/log_message>
#include <unistdx/base/make_object>
#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/fs/path>
#include <unistdx/io/fildes>
#include <unistdx/it/intersperse_iterator>

#define SUBORDINATION_ENV_APPLICATION_ID "SUBORDINATION_APPLICATION_ID"
#define SUBORDINATION_ENV_PIPE_IN "SUBORDINATION_PIPE_IN"
#define SUBORDINATION_ENV_PIPE_OUT "SUBORDINATION_PIPE_OUT"
#define SUBORDINATION_ENV_SLAVE "SUBORDINATION_MASTER"

namespace {

    std::independent_bits_engine<
        std::random_device,
        std::numeric_limits<char>::digits * sizeof(sbn::application_type),
        sbn::application_type> rng;

    sys::spin_mutex rng_mutex;

    inline sbn::application_type
    generate_application_id() noexcept {
        sys::simple_lock<sys::spin_mutex> lock(rng_mutex);
        return rng();
    }

    inline sbn::application_type
    get_appliction_id() noexcept {
        sbn::application_type id = 0;
        if (const char* s = ::getenv(SUBORDINATION_ENV_APPLICATION_ID)) {
            std::stringstream str(s);
            str >> id;
        }
        return id;
    }

    inline sys::fd_type
    get_pipe_fd(const char* name) noexcept {
        sys::fd_type fd = -1;
        if (const char* s = ::getenv(name)) {
            std::stringstream str(s);
            str >> fd;
        }
        return fd;
    }

    inline bool
    get_master() {
        return !std::getenv(SUBORDINATION_ENV_SLAVE);
    }

    sbn::application_type this_app = get_appliction_id();
    sys::fd_type this_pipe_in = get_pipe_fd(SUBORDINATION_ENV_PIPE_IN);
    sys::fd_type this_pipe_out = get_pipe_fd(SUBORDINATION_ENV_PIPE_OUT);
    bool this_is_master = get_master();

    template <class T>
    inline std::string
    generate_env(const char* key, const T& value) {
        std::stringstream str;
        str << key << '=' << value;
        return str.str();
    }

    /*
    inline std::string
    generate_filename(sbn::application_type app, const char* suffix) {
        std::stringstream filename;
        filename << app << suffix;
        return filename.str();
    }

    inline sys::fildes
    open_file(sbn::application_type app, const char* suffix) {
        return sys::fildes(
            generate_filename(app, suffix).data(),
            sys::open_flag::create | sys::open_flag::write_only,
            0644
        );
    }
    */

    void
    write_vector(sys::pstream& out, const std::vector<std::string>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (uint32_t i=0; i<n; ++i) {
            out << rhs[i];
        }
    }

    void
    read_vector(sys::pstream& in, std::vector<std::string>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.resize(n);
        for (uint32_t i=0; i<n; ++i) {
            in >> rhs[i];
        }
    }

}

namespace std {

    std::ostream&
    operator<<(std::ostream& out, const std::vector<std::string>& rhs) {
        std::copy(
            rhs.begin(),
            rhs.end(),
            sys::intersperse_iterator<std::string,char>(out, ',')
        );
        return out;
    }

}

std::ostream&
sbn::operator<<(std::ostream& out, const application& rhs) {
    return out << sys::make_object(
        "id",
        rhs._id,
        "uid",
        rhs._uid,
        "gid",
        rhs._gid,
        "args",
        rhs._args,
        "env",
        rhs._env,
        "wd",
        rhs._workdir,
        "role",
        rhs.role()
        );
}

sbn::application_type
sbn::this_application
::get_id() noexcept {
    return this_app;
}

sys::fd_type
sbn::this_application
::get_input_fd() noexcept {
    return this_pipe_in;
}

sys::fd_type
sbn::this_application
::get_output_fd() noexcept {
    return this_pipe_out;
}

bool
sbn::this_application
::is_master() noexcept {
    return this_is_master;
}

bool
sbn::this_application
::is_slave() noexcept {
    return !this_is_master;
}

sbn::application
::application(
    const container_type& args,
    const container_type& env
):
_id(generate_application_id()),
_uid(sys::this_process::user()),
_gid(sys::this_process::group()),
_args(args),
_env(env) {
    if (this->_args.empty()) {
        throw std::invalid_argument("empty arguments");
    }
}

int
sbn::application
::execute(const sys::two_way_pipe& pipe) const {
    sys::argstream args, env;
    for (const std::string& a : this->_args) {
        args.append(a);
    }
    for (const std::string& a : this->_env) {
        env.append(a);
    }
    // pass application ID
    env.append(generate_env(SUBORDINATION_ENV_APPLICATION_ID, this->_id));
    // pass in/out file descriptors
    env.append(generate_env(SUBORDINATION_ENV_PIPE_IN, pipe.child_in().fd()));
    env.append(
        generate_env(
            SUBORDINATION_ENV_PIPE_OUT,
            pipe.child_out().fd()
        )
    );
    // pass role
    if (this->is_slave()) {
        env.append(generate_env(SUBORDINATION_ENV_SLAVE, 1));
    }
    // update path to find executable files from user's PATH
    auto result =
        std::find_if(
            this->_env.begin(),
            this->_env.end(),
            [] (const std::string& rhs) {
                return rhs.find("PATH=") == 0;
            }
        );
    if (result == this->_env.end()) {
        UNISTDX_CHECK(::unsetenv("PATH"));
    } else {
        UNISTDX_CHECK(::putenv(const_cast<char*>(result->data())));
    }
    // disallow running as superuser/supergroup
    if (!this->_allowroot) {
        if (this->_uid == sys::superuser() || this->_gid == sys::supergroup()) {
            throw std::runtime_error("executing as superuser/supergroup is disallowed");
        }
    }
    sys::log_message("app", "execute");
    /*
    // redirect stdout/stderr
    sys::fildes outfd(STDOUT_FILENO), errfd(STDERR_FILENO);
    try {
        if (sys::fildes out{open_file(this->_id, ".out")}) {
            outfd = out;
        }
        if (sys::fildes err{open_file(this->_id, ".err")}) {
            errfd = err;
        }
    } catch (const sys::bad_call& err) {
        sys::log_message("app", "unable to redirect stdout/stderr");
    }*/
    //sys::log_message("app", "execute _", env);
    // switch user and group IDs
    if (sys::this_process::user() != this->_uid ||
        sys::this_process::group() != this->_gid) {
        sys::this_process::set_identity(this->_uid, this->_gid);
    }
    // change working directory
    if (!this->_workdir.empty()) {
        sys::this_process::workdir(this->_workdir);
    }
    sys::this_process::execute_command(args.argv(), env.argv());
    return 0;
}

void
sbn::swap(application& lhs, application& rhs) {
    std::swap(lhs._id, rhs._id);
    std::swap(lhs._uid, rhs._uid);
    std::swap(lhs._gid, rhs._gid);
    std::swap(lhs._args, rhs._args);
    std::swap(lhs._env, rhs._env);
    std::swap(lhs._workdir, rhs._workdir);
    std::swap(lhs._allowroot, rhs._allowroot);
}

void
sbn::application
::write(sys::pstream& out) const {
    out << this->_id << this->_uid << this->_gid;
    write_vector(out, this->_args);
    write_vector(out, this->_env);
    out << this->_workdir;
}

void
sbn::application
::read(sys::pstream& in) {
    in >> this->_id >> this->_uid >> this->_gid;
    read_vector(in, this->_args);
    read_vector(in, this->_env);
    in >> this->_workdir;
}

std::ostream&
sbn::operator<<(std::ostream& out, process_role_type rhs) {
    return out << (rhs == process_role_type::master ? "master" : "slave");
}
