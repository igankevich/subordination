using namespace factory;

const Port port = 40000;

struct Discoverer: public Kernel {

	void act() {
		auto neighbours = discover_neighbours();
		_num_neighbours = std::accumulate(neighbours.cbegin(), neighbours.cend(), uint32_t(0),
			[] (uint32_t sum, const Address_range& rhs)
		{
			return sum + rhs.end() - rhs.start();
		});
		std::for_each(neighbours.cbegin(), neighbours.cend(),
			[this] (const Address_range& range)
		{
			typedef typename Address_range::Int I;
			I st = range.start();
			I en = range.end();
			for (I a=st; a<en/* && a<st+10*/; ++a) {
				Endpoint ep(a, port);
				Discovery_kernel* k = new Discovery_kernel;
				k->parent(this);
				discovery_server()->send(k, ep);
			}
		});
	}

	void react(Kernel* k) {
		if (k->result() == Result::SUCCESS) {
			_returned_neighbours++;
		} else {
			_error_neighbours++;
		}
		std::clog << "Returned: "
			<< _returned_neighbours + _error_neighbours << '/' 
			<< _num_neighbours << std::endl;
		if (_returned_neighbours + _error_neighbours == _num_neighbours) {
			commit(the_server());
		}
	}

//	void error(Kernel* k) {
//		std::clog << "Returned ERR: " << _error_neighbours << std::endl;
//	}

private:
	uint32_t _num_neighbours = 0;
	uint32_t _returned_neighbours = 0;
	uint32_t _error_neighbours = 0;
};


struct App {
	int run(int, char**) {


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
			the_server()->send(new Discoverer);
//			std::this_thread::sleep_for(std::chrono::milliseconds(10000));
//			__factory.stop();
			__factory.wait();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};
