#include "application.hh"

#include <ostream>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <unistdx/base/check>
#include <unistdx/base/make_object>
#include <unistdx/base/n_random_bytes>

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

std::ostream&
factory::operator<<(std::ostream& out, const Application& rhs) {
	return out << sys::make_object("exec", rhs._execpath);
}

factory::application_type
factory::this_application::get_id() noexcept {
	return this_app;
}

factory::Application::Application(const path_type& exec):
_execpath(exec),
_id(generate_application_id())
{}

int
factory::Application::execute() const {
	std::stringstream str;
	str << FACTORY_ENV_APPLICATION_ID << '=' << this->_id;
	std::string s(str.str());
	UNISTDX_CHECK(::putenv(&s[0]));
	return sys::this_process::execute(this->_execpath);
}

