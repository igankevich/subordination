struct App {
	int run(int, char**) {

		using namespace factory;

		const Port port = 40000;
		std::cout << std::endl;
		std::cout << std::endl;
//		using namespace factory;
//		Endpoint ep = factory::random_endpoint(factory::discover_neighbours(), port);
//		std::cout << ep << std::endl;
//		return 0;
		try {
			the_server()->add(0);
			the_server()->start();
			discovery_server()->socket(Endpoint("0.0.0.0", port));
			discovery_server()->start();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));

			auto neighbours = discover_neighbours();
			std::for_each(neighbours.cbegin(), neighbours.cend(),
				[] (const Address_range& range)
			{
				typedef typename Address_range::Int I;
				I st = range.start();
				I en = range.end();
				for (I a=st; a<en && a<st+10; ++a) {
					Endpoint ep(a, port);
					discovery_server()->send(new Discovery_kernel, ep);
				}
			});
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			__factory.stop();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};
