namespace factory {

	extern Factory __factory;

	inline Local_server* the_server() { return __factory.local_server(); }
	inline Remote_server* remote_server() { return __factory.remote_server(); }
	inline External_server* ext_server() { return __factory.ext_server(); }
	inline Timer_server* timer_server() { return __factory.timer_server(); }
	inline App_server* app_server() { return __factory.app_server(); }
	inline Principal_server* principal_server() { return __factory.principal_server(); }

	namespace components {
		template<class App, class Endp, class Buf>
		void forward_to_app(App app, const Endp& from, Buf& buf) {
			app_server()->forward(app, from, buf);
		}
	}

}
