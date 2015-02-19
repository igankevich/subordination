struct App {
	int run(int, char**) {
		try {
			factory::find();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};
