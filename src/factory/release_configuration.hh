namespace factory {

	inline namespace configuration {

		typedef
			factory::components::Principal<
			factory::components::Mobile<
			factory::components::Basic_kernel>>
				Kernel;

		typedef factory::components::Tetris Tetris;
		typedef factory::components::Round_robin<Kernel> Round_robin;
		typedef factory::components::Simple_hashing<Kernel> Simple_hashing;


		typedef factory::components::Type<Kernel> Type;

		typedef factory::components::Service<Kernel> Service;

		template<class K>
			using Identifiable = factory::components::Identifiable<K, Type>;
		
		template<class K>
			using Server = factory::components::Server<K>;
	
		template<class K>
			using Server_server = factory::components::Server_server<Server<K>>;

		template<class X> using Pool = std::queue<X>;

		template<class S>
			using Rserver = factory::components::Rserver<Pool, S>;

		template<class Base, class Sub>
			using Iserver = factory::components::Iserver<Base, Sub>;

		template<class K>
			using Remote_Rserver = factory::components::Remote_Rserver<K, Pool, Type>;

		template<class K>
			using Socket_server = factory::components::Socket_server<Server<K>, Remote_Rserver<Kernel>, Kernel, Type, Pool>;

		template<class K>
			using Web_socket_server = factory::components::Web_socket_server<Server<K>, Type>;

		template<class Base>
			using Resource_aware = factory::components::Resource_aware<Base>;

		template<class Base, class S>
			using Profiled_Iserver = typename S::template Strategy<typename S::template Iprofiler<Base>>;

		template<class Base, class S, class S2>
			using Profiled_Iserver2 =
				typename S2::template Strategy<typename S2::template Iprofiler<
					S::template Strategy<typename S::template Iprofiler<Base>>>>;

		template<class Base, class S>
			using Profiled_Rserver = Rserver<typename S::template Rprofiler<Base>>;

		template<class Base, class S, class S2>
			using Profiled_Rserver2 =
				Rserver<typename S::template Rprofiler<
						typename S2::template Rprofiler<Base>>>;


//		template<class Base, class S>
//			using Profiled_Dserver = typename S::template Rprofiler<Dserver<typename S::template Rprofiler_top<Base>>>;

		typedef factory::components::Server_stack<
			Profiled_Iserver<Iserver<Server<Kernel>, Profiled_Rserver<Server<Kernel>, Round_robin>>, Round_robin>
//			Profiled_Iserver2<Iserver<Server<Kernel>, Profiled_Rserver2<Server<Kernel>, Round_robin, Simple_hashing<Kernel_pair>>>, Round_robin, Simple_hashing<Kernel_pair>>,
//			Profiled_Iserver<Iserver<Server<Kernel>, Profiled_Rserver<Server<Kernel>, Round_robin>>, Round_robin>,
//			Profiled_Iserver<Server<Kernel_pair>, Profiled_Dserver<Server<Kernel_pair>, Simple_hashing>, Simple_hashing>,
//			Profiled_Rserver<Server<Reader>, No_strategy>,
//			Server_server<Service>
			> Local_server;


		template<class K, class V>
			using Repository = factory::components::Repository<K, V>;

		typedef
			factory::components::Repository_stack<factory::components::Repository,
				int16_t, std::string,
				std::string, int16_t,
				Kernel*, Server<Kernel>*>
					Repository_stack;

		template<class T>
			using Mobile = factory::components::Type_init<T, Type, Kernel,
				factory::components::Kernel_link<T, factory::components::Remote<Kernel>>>;

		template<class T>
			using Unidentifiable = factory::components::Type_init<T, Type, Kernel, Kernel>;

		typedef factory::components::Discovery_kernel<Unidentifiable, Type> Discovery_kernel;

		typedef Socket_server<Kernel> Remote_server;
		typedef Socket_server<Discovery_kernel> Discovery_server;
	}

	using namespace configuration;

}
