namespace factory {

	namespace configuration {
	
		typedef factory::aspects::Reflecting_kernel<
			factory::aspects::Journaling_kernel<
				factory::aspects::Graphing_kernel<
					factory::components::Kernel>>>
						Kernel;

		typedef factory::components::Service<Kernel>
			Service;

		typedef factory::components::Scribe<Kernel> Scribe;

		template<class A> using Pool = factory::aspects::Journaling_pool<
			std::queue<A>>;

		typedef factory::components::Server<Kernel> Server;

		template<class U>
			using Rserver = factory::aspects::Journaling_Rserver<
				factory::components::Rserver<Server, Pool, Kernel, U>>;
	
		template<class U, class D>
			using Iserver = factory::components::Iserver<Server, Kernel, U, D>;

		typedef factory::components::Server_stack<Server, Kernel, Service> Server_stack;

		typedef factory::aspects::Journaling_production_strategy<
			factory::components::Tetris>
				Tetris;
	
		typedef factory::aspects::Journaling_production_strategy<
			factory::components::Stochastic_round_robin>
				Stochastic_round_robin;
	
		typedef factory::aspects::Journaling_production_strategy<
			factory::components::Round_robin>
				Round_robin;
	
		typedef factory::aspects::Journaling_reduction_strategy<
			factory::components::Simple_hashing>
				Simple_hashing;
	
	}

}
