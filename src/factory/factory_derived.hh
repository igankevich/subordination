namespace factory {

	Factory __factory;

	Local_server* the_server() { return __factory.local_server(); }
	Remote_server* remote_server() { return __factory.remote_server(); }
	External_server* ext_server() { return __factory.ext_server(); }
	Repository_stack* repository() { return __factory.repository(); }

	namespace components {

		void factory_stop() { __factory.stop(); }

		template<class K>
		void factory_send(K* kernel) { the_server()->send(kernel); }

		void factory_server_addr(std::ostream& out) {
			out << remote_server()->server_addr();
		}
	}

	void emergency_shutdown(int) {
		__factory.stop();
		static int num_calls = 0;
		static const int MAX_CALLS = 3;
		num_calls++;
		std::clog << "Ctrl-C shutdown." << std::endl;
		if (num_calls >= MAX_CALLS) {
			std::clog << "MAX_CALLS reached. Aborting." << std::endl;
			std::abort();
		}
	}

}
