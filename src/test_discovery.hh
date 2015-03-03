#include "process.hh"

using namespace factory;

typedef Interval<Port> Port_range;

const Port DEFAULT_PORT = 60000;
const int PORT_INCREMENT = 1;
const int NUM_SERVERS = 3;

const std::chrono::milliseconds SHUTDOWN_DELAY(3000);

struct Discovery: public Mobile<Discovery> {

	typedef uint64_t Time;
	typedef uint8_t State;

	Discovery() {}

	void act() {
//		std::cout << "Discovery!" << std::endl;
		commit(discovery_server());
	}

	void write_impl(Foreign_stream& out) {
		if (_state == 0) {
			_time = current_time();
		}
		_state++;
		out << _endpoint << _state << _time;
	}

	void read_impl(Foreign_stream& in) {
		in >> _endpoint >> _state >> _time;
		_state++;
		if (_state == 3) {
			_time = current_time() - _time;
		}
	}

	Time time() const { return _time; }
	void endpoint(Endpoint rhs) { _endpoint = rhs; }
	Endpoint endpoint() const { return _endpoint; }

	static Time current_time() {
		return std::chrono::steady_clock::now().time_since_epoch().count();
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:
	Time _time = 0;
	State _state = 0;
	Endpoint _endpoint;
};

struct Neighbour {

	typedef Discovery::Time Time;
	typedef uint32_t Metric;

	Neighbour(Endpoint a, Time t): _addr(a) { sample(t); }

	Metric metric() const { return _t/1000/1000/1000; }

	void sample(Time rhs) {
		if (++_n == MAX_SAMPLES) {
			_n = 1;
			_t = 0;
		}
		_t = ((_n-1)*_t + rhs) / _n;
	}

	bool operator<(const Neighbour& rhs) const {
		return metric() < rhs.metric()
			|| (metric() == rhs.metric() && _addr < rhs._addr);
	}

//	bool operator==(const Neighbour& rhs) const { return _t == rhs._t && _n == rhs._n; }

	friend std::ostream& operator<<(std::ostream& out, const Neighbour* rhs) {
		return out
			<< rhs->metric()
			<< ' ' << rhs->_n
			<< ' ' << rhs->_addr;
	}

private:
	Time _t = 0;
	uint32_t _n = 0;
	Endpoint _addr;

	static const uint32_t MAX_SAMPLES = 30;
};

struct Higher_metric {
	bool operator()(Neighbour* x, Neighbour* y) const {
		return *x < *y;
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
		_attempts++;
		_num_succeeded = 0;
		_num_failed = 0;

		std::cout << "Hello world from " << ::getpid() << ", " << _endpoint << std::endl;

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
		factory_log(Level::KERNEL) << "Sending discovery messages " << ::getpid() << ", " << _endpoint << std::endl;
		std::for_each(neighbours.cbegin(), neighbours.cend(),
			[this] (const Port_range& range)
		{
			typedef Port I;
			I st = range.start();
			I en = range.end();
			for (I a=st; a<en/* && a<st+10*/; ++a) {
				Endpoint ep(_endpoint.host(), a);
				Discovery* k = new Discovery;
				k->parent(this);
				k->endpoint(ep);
				discovery_server()->send(k, ep);
			}
		});
//		commit(the_server());
	}

	void react(Kernel* k) {
		Discovery* d = dynamic_cast<Discovery*>(k);
		if (k->result() == Result::SUCCESS) {
			_num_succeeded++;
		} else {
			_num_failed++;
		}
//		std::cout << "Returned: "
//			<< _num_succeeded + _num_failed << '/' 
//			<< _num_neighbours
//			<< " from " << d->endpoint() 
//			<< std::endl;
		
		if (_neighbours_map.find(d->endpoint()) == _neighbours_map.end()) {
			Neighbour* n = new Neighbour(d->endpoint(), d->time());
			_neighbours_map[d->endpoint()] = n;
			_neighbours_set.insert(n);
		} else {
			_neighbours_map[d->endpoint()]->sample(d->time());
		}

		if (_num_succeeded + _num_failed == _num_neighbours) {

			std::cout
				<< "Result = "
				<< num_succeeded()
				<< '+'
				<< num_failed()
				<< '/'
				<< num_neighbours()
				<< std::endl;

//			if (num_succeeded() == NUM_SERVERS-1 || _attempts == MAX_ATTEMPTS) {
			if (_attempts == MAX_ATTEMPTS) {
				std::cout << "Neighbours:" << std::endl;
				std::ostream_iterator<Neighbour*> it(std::cout, "\n");
				std::copy(_neighbours_set.cbegin(), _neighbours_set.cend(), it);
				std::this_thread::sleep_for(SHUTDOWN_DELAY);
				commit(the_server());
			} else {
				act();
			}
		}
	}

	uint32_t num_failed() const { return _num_failed; }
	uint32_t num_succeeded() const { return _num_succeeded; }
	uint32_t num_neighbours() const { return _num_neighbours; }

//	void error(Kernel* k) {
//		std::clog << "Returned ERR: " << _num_failed << std::endl;
//	}

private:
	Endpoint _endpoint;
	Port_range _port_range;

	std::map<Endpoint, Neighbour*> _neighbours_map;
	std::set<Neighbour*, Higher_metric> _neighbours_set;

	uint32_t _num_neighbours = 0;
	uint32_t _num_succeeded = 0;
	uint32_t _num_failed = 0;

	uint32_t _attempts = 0;

	static const uint32_t MAX_ATTEMPTS = 17;
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
				}
				retval = processes.wait();

				Process_id p1 = processes[0].id();
				Process_id p2 = processes[1].id();
				std::stringstream s;
				s << "paste -d';'";
				s << ' ';
				s << "/tmp/" << p1 << ".log";
				s << ' ';
				s << "/tmp/" << p2 << ".log";
				s << ' ';
				s << "| column -t -s';'";
				::system(s.str().c_str());
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
				the_server()->add(1);
				the_server()->start();
				discovery_server()->socket(endpoint);
				discovery_server()->start();
				factory_log(Level::SERVER) << "Sleeping." << std::endl;
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
