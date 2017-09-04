#include "application.hh"

#include <ostream>
#include <random>
#include <unistdx/base/make_object>
#include <unistdx/base/n_random_bytes>

#if !defined(FACTORY_DAEMON)
namespace {

	inline factory::application_type
	generate_appliction_id() noexcept {
		std::random_device rng;
		return sys::n_random_bytes<factory::application_type>(rng);
	}

	factory::application_type this_app = generate_appliction_id();

}
#endif

std::ostream&
factory::operator<<(std::ostream& out, const Application& rhs) {
	return out << sys::make_object("exec", rhs._execpath);
}

factory::application_type
factory::this_application::get_id() noexcept {
	#if defined(FACTORY_DAEMON)
	return 0;
	#else
	return 123;
	#endif
}
