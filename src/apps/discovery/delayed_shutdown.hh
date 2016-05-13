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
		#ifndef NDEBUG
		stdx::debug_message msg(stdx::dbg, "tst");
		#endif
		if (not _normal) {
			try {
				sys::endpoint addr = remote_server.server_addr();
				#ifndef NDEBUG
				msg << "killing " << addr;
				#endif
			} catch (...) {
				#ifndef NDEBUG
				msg << "killing this process";
				#endif
			}
			std::exit(0);
		} else {
			#ifndef NDEBUG
			msg << "shutdown gracefully this process";
			#endif
			factory::graceful_shutdown(0);
		}
	}

private:

	const hierarchy_type& _hierarchy;
	bool _normal;

};

#endif // APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
