#include "process.hh"

using namespace factory;

typedef Interval<Port> Port_range;

const Port DEFAULT_PORT = 60000;
const int PORT_INCREMENT = 1;
const int NUM_SERVERS = 2;

const std::chrono::milliseconds STARTUP_DELAY(2000);
const std::chrono::milliseconds SHUTDOWN_DELAY(4000);
const std::chrono::milliseconds INTERSPERSE_DELAY(500);

struct Discovery: public Mobile<Discovery> {

	Discovery() {}

	void act() {
		std::cout << "Discovery!" << std::endl;
		commit(discovery_server());
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}
};

struct Discoverer: public Identifiable<Kernel> {

//	void act() {
//		auto neighbours = discover_neighbours();
//		_num_neighbours = std::accumulate(neighbours.cbegin(), neighbours.cend(), uint32_t(0),
//			[] (uint32_t sum, const Address_range& rhs)
//		{
//			return sum + rhs.end() - rhs.start();
//		});
//		std::for_each(neighbours.cbegin(), neighbours.cend(),
//			[this] (const Address_range& range)
//		{
//			typedef typename Address_range::Int I;
//			I st = range.start();
//			I en = range.end();
//			for (I a=st; a<en/* && a<st+10*/; ++a) {
//				Endpoint ep(a, port);
//				std::cout << ep << std::endl;
//				Discovery_kernel* k = new Discovery_kernel;
//				k->parent(this);
//				discovery_server()->send(k, ep);
//			}
//		});
//	}

	Discoverer(Endpoint endpoint, Port_range port_range):
		_endpoint(endpoint), _port_range(port_range) {}

	void act() {

		std::cout << "Hello world from " << ::getpid() << ", " << _endpoint << std::endl;
//		commit(the_server());

		std::vector<Port_range> neighbours = {
			Port_range(_port_range.start(), _endpoint.port()),
			Port_range(_endpoint.port() + 1, _port_range.end())
		};

		// count neighbours
		_num_neighbours = std::accumulate(neighbours.cbegin(), neighbours.cend(), uint32_t(0),
			[] (uint32_t sum, const Port_range& rhs)
		{
			return sum + rhs.end() - rhs.start();
		});

		// send discovery messages
		std::for_each(neighbours.cbegin(), neighbours.cend(),
			[this] (const Port_range& range)
		{
			typedef Port I;
			I st = range.start();
			I en = range.end();
			for (I a=st; a<en/* && a<st+10*/; ++a) {
				Endpoint ep(_endpoint.host(), a);
				std::cout << ep << std::endl;
				Discovery* k = new Discovery;
				k->parent(this);
				discovery_server()->send(k, ep);
			}
		});
//		commit(the_server());
	}

	void react(Kernel* k) {
		if (k->result() == Result::SUCCESS) {
			_returned_neighbours++;
		} else {
			_error_neighbours++;
		}
		std::cout << "Returned: "
			<< _returned_neighbours + _error_neighbours << '/' 
			<< _num_neighbours
			<< " from " << k->from() 
			<< std::endl;
		if (_returned_neighbours + _error_neighbours == _num_neighbours) {
			std::this_thread::sleep_for(SHUTDOWN_DELAY);
			commit(the_server());
		}
	}

//	void error(Kernel* k) {
//		std::clog << "Returned ERR: " << _error_neighbours << std::endl;
//	}

private:
	Endpoint _endpoint;
	Port_range _port_range;

	uint32_t _num_neighbours = 0;
	uint32_t _returned_neighbours = 0;
	uint32_t _error_neighbours = 0;
};

template<class T>
std::string to_string(T rhs) {
	std::stringstream s;
	s << rhs;
	return s.str();
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc < 2) {
			try {
				std::vector<Endpoint> servers(NUM_SERVERS);
				for (int i=0; i<NUM_SERVERS; ++i) {
					servers[i] = Endpoint("127.0.0.1", DEFAULT_PORT + i*PORT_INCREMENT);
				}
				Port_range port_range(DEFAULT_PORT, DEFAULT_PORT + (NUM_SERVERS-1)*PORT_INCREMENT + 1);
				Process_group processes;
				for (int i=0; i<NUM_SERVERS; ++i) {
					Endpoint endpoint = servers[i];
					processes.add([endpoint, port_range, &argv] () {
						std::string arg1 = to_string(endpoint);
						std::string arg2 = to_string(port_range);
						char* const args[] = {argv[0], (char*)arg1.c_str(), (char*)arg2.c_str(), 0};
						char* const env[] = { 0 };
						check("execve()", ::execve(argv[0], args, env));
						return 0;
					});
					std::this_thread::sleep_for(SHUTDOWN_DELAY);
				}
				retval = processes.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		} else {
			std::stringstream cmdline;
			for (int i=1; i<argc; ++i) cmdline << argv[i] << ' ';
			Endpoint endpoint;
			Port_range port_range;
			cmdline >> endpoint >> port_range;
			try {
				the_server()->add(0);
				the_server()->start();
				discovery_server()->socket(endpoint);
				discovery_server()->start();
				factory_log(Level::SERVER) << "Sleeping." << std::endl;
				std::this_thread::sleep_for(STARTUP_DELAY);
				the_server()->send(new Discoverer(endpoint, port_range));
				__factory.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};
