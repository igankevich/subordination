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

#define FACTORY_ENV_APPLICATION_ID "FACTORY_APPLICATION_ID"

namespace {

	inline factory::application_type
	generate_application_id() noexcept {
		std::random_device rng;
		return sys::n_random_bytes<factory::application_type>(rng);
	}

	inline factory::application_type
	get_appliction_id() noexcept {
		#if defined(FACTORY_DAEMON)
		return 0;
		#else
		factory::application_type id = 0;
		if (const char* s = ::getenv(FACTORY_ENV_APPLICATION_ID)) {
			std::stringstream str(s);
			str >> id;
		}
		return id;
		#endif
	}

	factory::application_type this_app = get_appliction_id();

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
factory::operator<<(std::ostream& out, const Application& rhs) {
	return out << sys::make_object(
		"id", rhs._id,
		"uid", rhs._uid,
		"gid", rhs._gid,
		"args", rhs._args,
		"env", rhs._env
	);
}

factory::application_type
factory::this_application::get_id() noexcept {
	return this_app;
}

factory::Application::Application(
	const container_type& args,
	const container_type& env
):
_args(args),
_env(env)
{
	if (this->_args.empty()) {
		throw std::invalid_argument("empty arguments");
	}
}

int
factory::Application::execute() const {
	sys::argstream args, env;
	for (const std::string& a : this->_args) {
		args.append(a);
	}
	for (const std::string& a : this->_env) {
		env.append(a);
	}
	// set application ID
	std::stringstream str;
	str << FACTORY_ENV_APPLICATION_ID << '=' << this->_id;
	std::string s(str.str());
	env.append(str.str());
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
	// reset supplementary groups
	UNISTDX_CHECK(::setgroups(1, &this->_gid));
	// switch user and group IDs
	sys::this_process::set_identity(this->_uid, this->_gid);
	return sys::this_process::exec_command(args.argv(), env.argv());
}

