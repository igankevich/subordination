namespace factory {

	Factory __factory;

	Local_server* the_server() { return __factory.local_server(); }
	Remote_server* remote_server() { return __factory.remote_server(); }
	External_server* ext_server() { return __factory.ext_server(); }
	Timer_server* timer_server() { return __factory.timer_server(); }
	Repository_stack* repository() { return __factory.repository(); }

}
