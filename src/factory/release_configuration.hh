namespace factory {

	inline namespace configuration {

		typedef factory::components::Principal Kernel;

		typedef factory::components::Type<Kernel> Type;

		template<class K>
			using Identifiable = factory::components::Identifiable<K, Type>;
		
		template<class K>
			using Web_socket_server = factory::components::NIC_server<K, factory::components::Web_socket>;

		template<class T>
			using Mobile = factory::components::Type_init<T, Kernel>;

		template<class T>
			using Unidentifiable = factory::components::Type_init<T, Kernel>;

		typedef factory::components::CPU_server<Kernel> Local_server;
		typedef factory::components::NIC_server<Kernel, factory::unix::socket> Remote_server;
		typedef factory::components::Timer_server<Kernel> Timer_server;
		typedef factory::components::Sub_Iserver<factory::components::Server<Kernel>> App_server;
		typedef factory::components::Principal_server<factory::components::Server<Kernel>> Principal_server;

		typedef Web_socket_server<Kernel> External_server;

		typedef factory::components::Application Application;

		typedef factory::components::Basic_factory<
			Kernel,
			Local_server,
			Remote_server,
			External_server,
			Timer_server,
			App_server,
			Principal_server>
			Factory;

	}

	using namespace factory::configuration;

}
#define FACTORY_CONFIGURATION
