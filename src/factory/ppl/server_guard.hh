#ifndef FACTORY_SERVER_GUARD_HH
#define FACTORY_SERVER_GUARD_HH

namespace factory {

	inline void
	start_all() {}

	template <class Server, class ... Args>
	void
	start_all(Server& srv, Args& ... args) {
		srv.start();
		start_all(args...);
	}

	inline void
	stop_all() {}

	template <class Server, class ... Args>
	void
	stop_all(Server& srv, Args& ... args) {
		srv.stop();
		stop_all(args...);
	}

	inline void
	wait_all() {}

	template <class Server, class ... Args>
	void
	wait_all(Server& srv, Args& ... args) {
		srv.wait();
		wait_all(args...);
	}

}

#endif // FACTORY_SERVER_GUARD_HH
