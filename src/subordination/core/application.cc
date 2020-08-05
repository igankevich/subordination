#include <grp.h>
#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <utility>

#include <unistdx/base/check>
#include <unistdx/base/log_message>
#include <unistdx/base/make_object>
#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/fs/path>
#include <unistdx/io/fildes>
#include <unistdx/it/intersperse_iterator>

#include <subordination/core/application.hh>
#include <subordination/core/kernel_buffer.hh>

#define SUBORDINATION_ENV_APPLICATION_ID "SUBORDINATION_APPLICATION_ID"
#define SUBORDINATION_ENV_PIPE_IN "SUBORDINATION_PIPE_IN"
#define SUBORDINATION_ENV_PIPE_OUT "SUBORDINATION_PIPE_OUT"

namespace {

    std::independent_bits_engine<
        std::random_device,
        std::numeric_limits<char>::digits * sizeof(sbn::application::id_type),
        sbn::application::id_type> rng;

    sys::spin_mutex rng_mutex;

    inline sbn::application::id_type
    get_appliction_id() noexcept {
        sbn::application::id_type id = 0;
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

    sbn::application::id_type this_app = get_appliction_id();
    sys::fd_type this_pipe_in = get_pipe_fd(SUBORDINATION_ENV_PIPE_IN);
    sys::fd_type this_pipe_out = get_pipe_fd(SUBORDINATION_ENV_PIPE_OUT);

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

    template <class Stream> void
    write_vector(Stream& out, const std::vector<std::string>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (uint32_t i=0; i<n; ++i) {
            out << rhs[i];
        }
    }

    template <class Stream> void
    read_vector(Stream& in, std::vector<std::string>& rhs) {
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
        "id", rhs._id,
        "uid", rhs._uid,
        "gid", rhs._gid,
        "args", rhs._args,
        //"env", rhs._env,
        "wd", rhs._working_directory);
}

sbn::application::id_type sbn::this_application::id() noexcept { return this_app; }
void sbn::this_application::id(application::id_type rhs) noexcept { this_app = rhs; }
sys::fd_type sbn::this_application::get_input_fd() noexcept { return this_pipe_in; }
sys::fd_type sbn::this_application::get_output_fd() noexcept { return this_pipe_out; }
bool sbn::this_application::standalone() noexcept { return !std::getenv(SUBORDINATION_ENV_APPLICATION_ID); }

sbn::application::id_type
sbn::generate_application_id() noexcept {
    sys::simple_lock<sys::spin_mutex> lock(rng_mutex);
    return rng();
}

sbn::application
::application(const string_array& args, const string_array& env):
_id(generate_application_id()),
_uid(sys::this_process::user()),
_gid(sys::this_process::group()),
_args(args),
_env(env) {
    if (this->_args.empty()) {
        throw std::invalid_argument("empty arguments");
    }
}

int sbn::application::execute(const sys::two_way_pipe& pipe) const {
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
    env.append(generate_env(SUBORDINATION_ENV_PIPE_OUT, pipe.child_out().fd()));
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
    sys::this_process::set_identity(this->_uid, this->_gid);
    // change working directory
    if (!this->_working_directory.empty()) {
        sys::this_process::workdir(this->_working_directory);
    }
    sys::this_process::execute_command(args.argv(), env.argv());
    return 0;
}

void sbn::application::write(kernel_buffer& out) const {
    out << this->_id << this->_uid << this->_gid;
    write_vector(out, this->_args);
    write_vector(out, this->_env);
    out << this->_working_directory;
}

void sbn::application::read(kernel_buffer& in) {
    in >> this->_id >> this->_uid >> this->_gid;
    read_vector(in, this->_args);
    read_vector(in, this->_env);
    in >> this->_working_directory;
}

sbn::kernel_buffer& sbn::operator<<(kernel_buffer& out, const application& rhs) {
    rhs.write(out); return out;
}

sbn::kernel_buffer& sbn::operator>>(kernel_buffer& in, application& rhs) {
    rhs.read(in); return in;
}
