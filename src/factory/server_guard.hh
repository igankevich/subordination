#ifndef FACTORY_SERVER_GUARD_HH
#define FACTORY_SERVER_GUARD_HH

namespace factory {

	template<class Server>
	struct Server_guard {

		Server_guard(Server& rhs):
		_server(rhs)
		{ _server.start(); }

		~Server_guard() {
			_server.stop();
			_server.wait();
		}

	private:

		Server& _server;

	};

}

#endif // FACTORY_SERVER_GUARD_HH
