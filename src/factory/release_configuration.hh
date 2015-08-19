namespace factory {

	inline namespace configuration {

		typedef
			factory::components::Principal<
			factory::components::Mobile<
			factory::components::Basic_kernel>>
				Kernel;

//		typedef factory::components::Tetris Tetris;
//		typedef factory::components::Round_robin<Kernel> Round_robin;
//		typedef factory::components::Simple_hashing<Kernel> Simple_hashing;


		typedef factory::components::Type<Kernel> Type;

		template<class K>
			using Identifiable = factory::components::Identifiable<K, Type>;
		
		template<class K>
			using Server = factory::components::Server<K>;
	
//		template<class X> using Pool = std::queue<X>;

//		template<class S>
//			using Rserver = factory::components::Rserver<Pool, S>;
//
//		template<class Base, class Sub>
//			using Iserver = factory::components::Iserver<Base, Sub>;

//		template<class K>
//			using Remote_Rserver = factory::components::Remote_Rserver<K, Pool, factory::unix::socket>;

//		template<class K>
//			using Web_socket_remote_Rserver = factory::components::Remote_Rserver<K, Pool, factory::components::Web_socket>;

		template<class K>
			using Web_socket_server = factory::components::NIC_server<K, factory::components::Web_socket>;

//		template<class Base, class S>
//			using Profiled_Iserver = typename S::template Strategy<typename S::template Iprofiler<Base>>;

//		template<class Base, class S, class S2>
//			using Profiled_Iserver2 =
//				typename S2::template Strategy<typename S2::template Iprofiler<
//					S::template Strategy<typename S::template Iprofiler<Base>>>>;

//		template<class Base, class S>
//			using Profiled_Rserver = Rserver<typename S::template Rprofiler<Base>>;

//		template<class Base, class S, class S2>
//			using Profiled_Rserver2 =
//				Rserver<typename S::template Rprofiler<
//						typename S2::template Rprofiler<Base>>>;


//		template<class Base, class S>
//			using Profiled_Dserver = typename S::template Rprofiler<Dserver<typename S::template Rprofiler_top<Base>>>;

//		typedef Profiled_Iserver<Iserver<Server<Kernel>, Profiled_Rserver<Server<Kernel>, Round_robin>>, Round_robin>
//			Local_server;


		template<class T>
			using Mobile = factory::components::Type_init<T, Type, Kernel,
				factory::components::Kernel_link<T, Kernel>>;

		template<class T>
			using Unidentifiable = factory::components::Type_init<T, Type, Kernel, Kernel>;

		typedef factory::components::CPU_server<Kernel> Local_server;
		typedef factory::components::NIC_server<Kernel, factory::unix::socket> Remote_server;
		typedef factory::components::Timer_server<Kernel> Timer_server;
		typedef factory::components::Sub_Iserver<Server<Kernel>> App_server;
		typedef factory::components::Principal_server<Server<Kernel>> Principal_server;

		typedef Web_socket_server<Kernel> External_server;

		typedef factory::components::Shutdown<Mobile, Type> Shutdown;
		typedef factory::components::Application Application;

		typedef factory::components::Basic_factory<
			Local_server,
			Remote_server,
			External_server,
			Timer_server,
			App_server,
			Principal_server,
			Shutdown>
			Factory;

//		typedef factory::components::Basic_topology<unix::endpoint, uint16_t> Topology;
	}

	using namespace factory::configuration;

}
#define FACTORY_CONFIGURATION
