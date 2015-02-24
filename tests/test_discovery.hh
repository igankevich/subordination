struct App {
	int run(int, char**) {
		std::cout << std::endl;
		std::cout << std::endl;
		try {
			using namespace factory;
			the_server()->add(0);
			the_server()->start();
			discovery_server()->socket(Endpoint("0.0.0.0", 40000));
			discovery_server()->start();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			// TODO: add endpoint
			discovery_server()->send(new Discovery_kernel);
			std::this_thread::sleep_for(std::chrono::milliseconds(10000));
			__factory.stop();
//			factory::discover_neighbours();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};
