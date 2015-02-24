namespace factory {

	Factory __factory;

	Local_server* the_server() { return __factory.local_server(); }
	Remote_server* remote_server() { return __factory.remote_server(); }
	Discovery_server* discovery_server() { return __factory.discovery_server(); }
	Repository_stack* repository() { return __factory.repository(); }

	namespace components {

		void factory_stop() { __factory.stop(); }

		template<class K>
		void factory_send(K* kernel) { the_server()->send(kernel); }

		template<class K>
		void discovery_send(K* kernel, Endpoint endp) { discovery_server()->send(kernel, endp); }

	}

	void emergency_shutdown(int signal) {
		__factory.stop();
		static int num_calls = 0;
		static const int MAX_CALLS = 3;
		num_calls++;
		std::clog << "EMERGENCY shutdown." << std::endl;
		if (signal == SIGFPE) {
			std::clog << "Arithmetic exception caught." << std::endl;
		}
		if (num_calls >= MAX_CALLS) {
			std::clog << "MAX_CALLS reached. Aborting." << std::endl;
			std::abort();
		}
	}

}
