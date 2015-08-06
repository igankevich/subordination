namespace factory {

	extern Factory __factory;

	inline Local_server* the_server() { return __factory.local_server(); }
	inline Remote_server* remote_server() { return __factory.remote_server(); }
	inline External_server* ext_server() { return __factory.ext_server(); }
	inline Timer_server* timer_server() { return __factory.timer_server(); }
	inline App_server* app_server() { return __factory.app_server(); }
	inline Repository_stack* repository() { return __factory.repository(); }

	namespace components {
		template<class App, class Buf, class Stream>
		void forward_to_app(App app, Buf& buf, Stream& str) {
			app_server()->forward(app, buf, str);
		}
	}

}
