#ifndef FACTORY_SERVER_GUARD_HH
#define FACTORY_SERVER_GUARD_HH

namespace factory {

	template<class Server>
	struct Server_guard: public Server {

		template<class ... Args>
		Server_guard(Args&& ... args):
		Server(std::forward<Args>(args)...)
		{ this->start(); }

		virtual
		~Server_guard() {
			this->shutdown();
			this->stop();
			this->wait();
		}

	};

}

#endif // FACTORY_SERVER_GUARD_HH
