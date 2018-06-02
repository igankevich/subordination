#include "application.hh"

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

#include <bscheduler/config.hh>

#define BSCHEDULER_ENV_APPLICATION_ID "BSCHEDULER_APPLICATION_ID"
#define BSCHEDULER_ENV_PIPE_IN "BSCHEDULER_PIPE_IN"
#define BSCHEDULER_ENV_PIPE_OUT "BSCHEDULER_PIPE_OUT"
#define BSCHEDULER_ENV_SLAVE "BSCHEDULER_MASTER"

namespace {

	std::independent_bits_engine<
		std::random_device,
		std::numeric_limits<char>::digits * sizeof(bsc::application_type),
		bsc::application_type> rng;

	sys::spin_mutex rng_mutex;

	inline bsc::application_type
	generate_application_id() noexcept {
		sys::simple_lock<sys::spin_mutex> lock(rng_mutex);
		return rng();
	}

	inline bsc::application_type
	get_appliction_id() noexcept {
		bsc::application_type id = 0;
		if (const char* s = ::getenv(BSCHEDULER_ENV_APPLICATION_ID)) {
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
		return !std::getenv(BSCHEDULER_ENV_SLAVE);
	}

	bsc::application_type this_app = get_appliction_id();
	sys::fd_type this_pipe_in = get_pipe_fd(BSCHEDULER_ENV_PIPE_IN);
	sys::fd_type this_pipe_out = get_pipe_fd(BSCHEDULER_ENV_PIPE_OUT);
	bool this_is_master = get_master();

	template <class T>
	inline std::string
	generate_env(const char* key, const T& value) {
		std::stringstream str;
		str << key << '=' << value;
		return str.str();
	}

	inline std::string
	generate_filename(bsc::application_type app, const char* suffix) {
		std::stringstream filename;
		filename
		    << BSCHEDULER_LOG_DIRECTORY
		    << sys::file_separator
		    << app
		    << suffix;
		return filename.str();
	}

	inline sys::fildes
	open_file(bsc::application_type app, const char* suffix) {
		return sys::fildes(
			generate_filename(app, suffix).data(),
			sys::open_flag::create | sys::open_flag::write_only,
			0644
		);
	}

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
bsc::operator<<(std::ostream& out, const application& rhs) {
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

bsc::application_type
bsc::this_application
::get_id() noexcept {
	return this_app;
}

sys::fd_type
bsc::this_application
::get_input_fd() noexcept {
	return this_pipe_in;
}

sys::fd_type
bsc::this_application
::get_output_fd() noexcept {
	return this_pipe_out;
}

bool
bsc::this_application
::is_master() noexcept {
	return this_is_master;
}

bool
bsc::this_application
::is_slave() noexcept {
	return !this_is_master;
}

bsc::application
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
bsc::application
::execute(const sys::two_way_pipe& pipe) const {
	sys::argstream args, env;
	for (const std::string& a : this->_args) {
		args.append(a);
	}
	for (const std::string& a : this->_env) {
		env.append(a);
	}
	// pass application ID
	env.append(generate_env(BSCHEDULER_ENV_APPLICATION_ID, this->_id));
	// pass in/out file descriptors
	env.append(generate_env(BSCHEDULER_ENV_PIPE_IN, pipe.child_in().fd()));
	env.append(
		generate_env(
			BSCHEDULER_ENV_PIPE_OUT,
			pipe.child_out().fd()
		)
	);
	// pass role
	if (this->is_slave()) {
		env.append(generate_env(BSCHEDULER_ENV_SLAVE, 1));
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
			throw std::runtime_error(
					  "executing as superuser/supergroup is disallowed"
			);
		}
	}
	// redirect stdout/stderr
	sys::fildes outfd, errfd;
	try {
		outfd = std::move(open_file(this->_id, ".out"));
		errfd = std::move(open_file(this->_id, ".err"));
		if (outfd) {
			outfd.remap(STDOUT_FILENO);
		}
		if (errfd) {
			errfd.remap(STDERR_FILENO);
		}
	} catch (const std::exception& err) {
		sys::log_message("app", "unable to redirect stdout/stderr");
	}
	// switch user and group IDs
	sys::this_process::set_identity(this->_uid, this->_gid);
	// change working directory
	if (!this->_workdir.empty()) {
		sys::this_process::workdir(this->_workdir);
	}
	return sys::this_process::execute_command(args.argv(), env.argv());
}

void
bsc
::swap(application& lhs, application& rhs) {
	std::swap(lhs._id, rhs._id);
	std::swap(lhs._uid, rhs._uid);
	std::swap(lhs._gid, rhs._gid);
	std::swap(lhs._args, rhs._args);
	std::swap(lhs._env, rhs._env);
	std::swap(lhs._workdir, rhs._workdir);
	std::swap(lhs._allowroot, rhs._allowroot);
}

void
bsc::application
::write(sys::pstream& out) const {
	out << this->_id << this->_uid << this->_gid;
	write_vector(out, this->_args);
	write_vector(out, this->_env);
	out << this->_workdir;
}

void
bsc::application
::read(sys::pstream& in) {
	in >> this->_id >> this->_uid >> this->_gid;
	read_vector(in, this->_args);
	read_vector(in, this->_env);
	in >> this->_workdir;
}

std::ostream&
bsc::operator<<(std::ostream& out, process_role_type rhs) {
	return out << (rhs == process_role_type::master ? "master" : "slave");
}

