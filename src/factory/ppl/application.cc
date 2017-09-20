#include "application.hh"

#include <algorithm>
#include <ostream>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <grp.h>
#include <unistdx/base/check>
#include <unistdx/base/log_message>
#include <unistdx/base/make_object>
#include <unistdx/base/n_random_bytes>
#include <unistdx/it/intersperse_iterator>
#include <unistdx/fs/path>
#include <unistdx/io/fildes>

#include <factory/config.hh>

#define FACTORY_ENV_APPLICATION_ID "FACTORY_APPLICATION_ID"
#define FACTORY_ENV_PIPE_IN "FACTORY_PIPE_IN"
#define FACTORY_ENV_PIPE_OUT "FACTORY_PIPE_OUT"

namespace {

	inline asc::application_type
	generate_application_id() noexcept {
		std::random_device rng;
		return sys::n_random_bytes<asc::application_type>(rng);
	}

	inline asc::application_type
	get_appliction_id() noexcept {
		asc::application_type id = 0;
		if (const char* s = ::getenv(FACTORY_ENV_APPLICATION_ID)) {
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

	asc::application_type this_app = get_appliction_id();
	sys::fd_type this_pipe_in = get_pipe_fd(FACTORY_ENV_PIPE_IN);
	sys::fd_type this_pipe_out = get_pipe_fd(FACTORY_ENV_PIPE_OUT);

	template <class T>
	inline std::string
	generate_env(const char* key, const T& value) {
		std::stringstream str;
		str << key << '=' << value;
		return str.str();
	}

	inline std::string
	generate_filename(asc::application_type app, const char* suffix) {
		std::stringstream filename;
		filename
			<< FACTORY_LOG_DIRECTORY
			<< sys::file_separator
			<< app
			<< suffix;
		return filename.str();
	}

	inline sys::fildes
	open_file(asc::application_type app, const char* suffix) {
		return sys::fildes(
			generate_filename(app, suffix).data(),
			sys::open_flag::create | sys::open_flag::write_only,
			0644
		);
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
asc::operator<<(std::ostream& out, const application& rhs) {
	return out << sys::make_object(
		"id", rhs._id,
		"uid", rhs._uid,
		"gid", rhs._gid,
		"args", rhs._args,
		"env", rhs._env
	);
}

asc::application_type
asc::this_application::get_id() noexcept {
	return this_app;
}


sys::fd_type
asc::this_application::get_input_fd() noexcept {
	return this_pipe_in;
}

sys::fd_type
asc::this_application::get_output_fd() noexcept {
	return this_pipe_out;
}

asc::application::application(
	const container_type& args,
	const container_type& env
):
_id(generate_application_id()),
_uid(sys::this_process::user()),
_gid(sys::this_process::group()),
_args(args),
_env(env)
{
	if (this->_args.empty()) {
		throw std::invalid_argument("empty arguments");
	}
}

int
asc::application::execute(const sys::two_way_pipe& pipe) const {
	sys::argstream args, env;
	for (const std::string& a : this->_args) {
		args.append(a);
	}
	for (const std::string& a : this->_env) {
		env.append(a);
	}
	// pass application ID
	env.append(generate_env(FACTORY_ENV_APPLICATION_ID, this->_id));
	// pass in/out file descriptors
	env.append(generate_env(FACTORY_ENV_PIPE_IN, pipe.child_in().get_fd()));
	env.append(generate_env(FACTORY_ENV_PIPE_OUT, pipe.child_out().get_fd()));
	// update path to find executable files from user's PATH
	auto result = std::find_if(
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
	if (this->_uid == sys::superuser() || this->_gid == sys::supergroup()) {
		throw std::runtime_error("executing as superuser/supergroup is disallowed");
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
	return sys::this_process::exec_command(args.argv(), env.argv());
}

