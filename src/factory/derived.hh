#ifdef FACTORY_NO_DERIVED_SYSTEM
namespace factory {
	namespace components {

		void factory_stop(bool) {}

		void factory_server_addr(std::ostream& out) {
			out << "0.0.0.0";
		}
	}
}
#else
namespace factory {

	Factory __factory;

	Local_server* the_server() { return __factory.local_server(); }
	Remote_server* remote_server() { return __factory.remote_server(); }
	External_server* ext_server() { return __factory.ext_server(); }
	Timer_server* timer_server() { return __factory.timer_server(); }
	Repository_stack* repository() { return __factory.repository(); }

	namespace components {

		void factory_stop(bool force) {
			if (force) {
				__factory.stop_now();
			} else {
				__factory.stop();
			}
		}

		void factory_server_addr(std::ostream& out) {
			out << remote_server()->server_addr();
		}
	}

}
#endif
