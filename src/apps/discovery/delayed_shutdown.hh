#ifndef APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
#define APPS_DISCOVERY_DELAYED_SHUTDOWN_HH

#include <factory/api.hh>

template<class Address>
struct Delayed_shutdown: public Kernel {

	typedef Address addr_type;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;

	explicit
	Delayed_shutdown(const hierarchy_type& peers, bool normal) noexcept:
	_hierarchy(peers),
	_normal(normal)
	{}

	void
	act() override {
		using namespace factory::api;
		if (not _normal) {
			try {
				#ifndef NDEBUG
				std::stringstream msg;
				std::copy(
					factory::factory.nic().ifaddrs_begin(),
					factory::factory.nic().ifaddrs_end(),
					sys::intersperse_iterator<ifaddr_type,char>(msg, ',')
				);
				sys::log_message("tst", "killing _", msg.str());
				#endif
			} catch (...) {
				#ifndef NDEBUG
				sys::log_message("tst", "killing this process");
				#endif
			}
			std::exit(0);
		} else {
			#ifndef NDEBUG
			sys::log_message("tst", "shutdown this process");
			#endif
			factory::graceful_shutdown(0);
		}
	}

private:

	const hierarchy_type& _hierarchy;
	bool _normal;

};

#endif // vim:filetype=cpp
