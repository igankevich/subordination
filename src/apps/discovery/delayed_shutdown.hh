#ifndef APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
#define APPS_DISCOVERY_DELAYED_SHUTDOWN_HH

#include <sysx/signal.hh>

template<class Address>
struct Delayed_shutdown: public Kernel {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	typedef stdx::log<Delayed_shutdown> this_log;

	explicit
	Delayed_shutdown(const hierarchy_type& peers, sysx::signal sig) noexcept:
	_hierarchy(peers),
	_signal(sig)
	{}

	void
	act(Server& this_server) override {
//		this_log() << "Hail the king! His hoes: " << _hierarchy << std::endl;
//		this_server.factory()->shutdown();
		sysx::this_process::send(_signal);
	}

private:

	const hierarchy_type& _hierarchy;
	sysx::signal _signal;

};

#endif // APPS_DISCOVERY_DELAYED_SHUTDOWN_HH
