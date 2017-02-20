#ifndef APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
#define APPS_DISCOVERY_DELAYED_SHUTDOWN_HH

template<class Address>
struct Delayed_shutdown: public Kernel {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;

	explicit
	Delayed_shutdown(const hierarchy_type& peers, bool normal) noexcept:
	_hierarchy(peers),
	_normal(normal)
	{}

	void
	act() override {
		if (not _normal) {
			try {
				#ifndef NDEBUG
				sys::endpoint addr = remote_server.server_addr();
				stdx::debug_message("tst", "killing _", addr);
				#endif
			} catch (...) {
				#ifndef NDEBUG
				stdx::debug_message("tst", "killing this process");
				#endif
			}
			std::quick_exit(0);
		} else {
			#ifndef NDEBUG
			stdx::debug_message("tst", "shutdown this process");
			#endif
			factory::graceful_shutdown(0);
		}
	}

private:

	const hierarchy_type& _hierarchy;
	bool _normal;

};

#endif // APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
