#ifndef APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
#define APPS_DISCOVERY_DELAYED_SHUTDOWN_HH

template<class Address>
struct Delayed_shutdown: public Kernel {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	typedef stdx::log<Delayed_shutdown> this_log;

	explicit
	Delayed_shutdown(const hierarchy_type& peers, bool normal) noexcept:
	_hierarchy(peers),
	_normal(normal)
	{}

	void
	act() override {
		if (not _normal) {
			try {
				sysx::endpoint addr = factory()->remote_server()->server_addr();
				this_log() << "Killing " << addr << std::endl;
			} catch (...) {
				this_log() << "Killing this process" << std::endl;
			}
			std::quick_exit(0);
		} else {
			factory()->shutdown();
		}
	}

private:

	const hierarchy_type& _hierarchy;
	bool _normal;

};

#endif // APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
