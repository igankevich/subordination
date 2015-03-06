#include "process.hh"

using namespace factory;

typedef Interval<Port> Port_range;

const Port DEFAULT_PORT = 10000;
const int PORT_INCREMENT = 1;
const int NUM_SERVERS = 3;

std::vector<Endpoint> NEIGHBOURS = {
	Endpoint("127.0.0.1", DEFAULT_PORT),
	Endpoint("127.0.0.2", DEFAULT_PORT),
	Endpoint("127.0.0.3", DEFAULT_PORT)
};

struct Discovery: public Mobile<Discovery> {

	typedef uint64_t Time;
	typedef uint8_t State;

	Discovery() {}

	void act() {
		commit(discovery_server());
	}

	void write_impl(Foreign_stream& out) {
		if (_state == 0) {
			_time = current_time();
		}
		_state++;
		out << _src << _dest << _state << _time;
	}

	void read_impl(Foreign_stream& in) {
		in >> _src >> _dest >> _state >> _time;
		_state++;
		if (_state == 3) {
			_time = current_time() - _time;
		}
	}

	Time time() const { return _time; }

	void dest(Endpoint rhs) { _dest = rhs; }
	Endpoint dest() const { return _dest; }
	void source(Endpoint rhs) { _src = rhs; }
	Endpoint source() const { return _src; }

	static Time current_time() {
		return std::chrono::steady_clock::now().time_since_epoch().count();
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:
	Time _time = 0;
	State _state = 0;
	Endpoint _src;
	Endpoint _dest;
};

struct Neighbour {

	typedef Discovery::Time Time;
	typedef uint32_t Metric;

	Neighbour(Endpoint a, Time t): _addr(a) { sample(t); }
	Neighbour(const Neighbour& rhs):
		_t(rhs._t), _n(rhs._n), _addr(rhs._addr) {}

	Metric metric() const { return _t/1000/1000/1000; }

	void sample(Time rhs) {
		if (++_n == MAX_SAMPLES) {
			_n = 1;
			_t = 0;
		}
		_t = ((_n-1)*_t + rhs) / _n;
	}

	uint32_t num_samples() const { return _n; }
	Endpoint addr() const { return _addr; }

	bool operator<(const Neighbour& rhs) const {
		return metric() < rhs.metric()
			|| (metric() == rhs.metric() && _addr < rhs._addr);
	}

	Neighbour& operator=(const Neighbour& rhs) {
		_t = rhs._t;
		_n = rhs._n;
		_addr = rhs._addr;
		return *this;
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

	static const uint32_t MAX_SAMPLES = 1000;
};

struct Higher_metric {
	bool operator()(Neighbour* x, Neighbour* y) const {
		return *x < *y;
	}
};

struct Ballot: public Mobile<Ballot> {

	Ballot() {}

	void source(Endpoint rhs) { _src = rhs; }
	Endpoint source() const { return _src; }

	void vote(bool rhs) { _vote = rhs ? 0 : 1; }
	bool vote() const { return _vote == 0; }
	
	static void init_type(Type* t) {
		t->id(3);
	}

	void write_impl(Foreign_stream& out) { out << _vote << _src; }
	void read_impl(Foreign_stream& in) { in >> _vote >> _src; }

private:
	Endpoint _src;
	uint8_t _vote = 0;
};

struct Candidate: public Identifiable<Kernel> {
	
	Candidate(Endpoint src, const std::set<Neighbour>& nb):
		factory::Identifiable<Kernel>(2),
		_source(src),
		_neighbours(nb) {}
	
	void act() {
		std::cout << "Starting poll " << _source << std::endl;
		std::for_each(_neighbours.cbegin(), _neighbours.cend(),
			[this] (const Neighbour& rhs)
		{
			Ballot* k = new Ballot;
			k->parent(this);
			k->principal(this); // the same id
			k->source(_source);
			discovery_server()->send(k, rhs.addr());
		});
	}

	void react(factory::Kernel* k) {
		Ballot* b = dynamic_cast<Ballot*>(k);
		// ballot from another host
		if (b->source() != _source) {
			// the address of the neighbour with the highest rank is the same
			// as the address of the candidate
			Ballot* bb = new Ballot;
			bb->parent(this);
			bb->principal(this);
			bb->from(b->from());
			bb->source(b->source());
			bb->vote(b->source() == _neighbours.cbegin()->addr());
			bb->result(Result::SUCCESS);
			discovery_server()->send(bb, b->from());
		} else {
			// returning ballot
			if (b->result() == Result::SUCCESS) {
				if (b->vote()) _num_for++; else _num_against++;
			} else {
				_num_neutral++;
			}
			// all ballots have been returned
			if (_num_for + _num_against + _num_neutral == _neighbours.size()) {
				std::cout << _source << " exit poll: "
					<< _num_for << '+'
					<< _num_against << '+'
					<< _num_neutral << '/'
					<< _neighbours.size()
					<< std::endl;
//				commit(the_server());
			}
		}
		std::cout << _source << " preliminary: "
			<< _num_for << '+'
			<< _num_against << '+'
			<< _num_neutral << '/'
			<< _neighbours.size()
			<< std::endl;
	}

private:
	Endpoint _source;
	std::set<Neighbour> _neighbours;
	
	uint32_t _num_for = 0;
	uint32_t _num_against = 0;
	uint32_t _num_neutral = 0;
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
		factory::Identifiable<Kernel>(1),
		_source(endpoint), _port_range(port_range) {}

	void act() {
		_attempts++;
		_num_succeeded = 0;
		_num_failed = 0;
		_num_neighbours = 0;

//		std::cout << "Hello world from " << ::getpid() << ", " << _source << std::endl;
		std::cout << _source << ": attempt #" << _attempts << std::endl;

//		std::vector<Port_range> neighbours = {
//			Port_range(_port_range.start(), _source.port()),
//			Port_range(_source.port() + 1, _port_range.end())
//		};

		std::vector<Discovery*> kernels;


		std::for_each(NEIGHBOURS.cbegin(), NEIGHBOURS.cend(),
			[this, &kernels] (const Endpoint& ep)
		{
			Neighbour* n = find_neighbour(ep);
			std::cout 
				<< _source
				<< ": sending to " << ep << ' '
				<< ((n != nullptr) ? n->num_samples() : 0) << std::endl;
			if (n == nullptr || n->num_samples() < MIN_SAMPLES) {
				factory_log(Level::KERNEL) << "Sending to " << ep << std::endl;
				Discovery* k = new Discovery;
				k->parent(this);
				k->dest(ep);
				k->source(_source);
				kernels.push_back(k);
			}
		});


		// determine hosts to poll
//		std::for_each(neighbours.cbegin(), neighbours.cend(),
//			[this, &kernels] (const Port_range& range)
//		{
//			typedef Port I;
//			I st = range.start();
//			I en = range.end();
//			for (I a=st; a<en/* && a<st+10*/; ++a) {
//				Endpoint ep(_source.host(), a);
//				Neighbour* n = find_neighbour(ep);
//				std::cout 
//					<< _source
//					<< ": sending to " << ep << ' '
//					<< ((n != nullptr) ? n->num_samples() : 0) << std::endl;
//				if (n == nullptr || n->num_samples() < MIN_SAMPLES) {
//					factory_log(Level::KERNEL) << "Sending to " << ep << std::endl;
//					Discovery* k = new Discovery;
//					k->parent(this);
//					k->dest(ep);
//					k->source(_source);
//					kernels.push_back(k);
//				}
//			}
//		});

		// send discovery messages
		factory_log(Level::KERNEL) << "Sending discovery messages " << ::getpid() << ", " << _source << std::endl;
		_num_neighbours = kernels.size();
		for (Discovery* d : kernels) {
			discovery_server()->send(d, d->dest());
		}

		// repeat when nothing is discovered
		if (_num_neighbours == 0) {
			throw "No neighbours to update";
//			the_server()->send(this);
		}
	}

	void react(Kernel* k) {
		Discovery* d = dynamic_cast<Discovery*>(k);
		if (d->result() == Result::SUCCESS) {
			std::cout << _source << ": success for " << d->dest() << std::endl;
			_num_succeeded++;
			if (_neighbours_map.find(d->dest()) == _neighbours_map.end()) {
				Neighbour* n = new Neighbour(d->dest(), d->time());
				_neighbours_map[d->dest()] = n;
				_neighbours_set.insert(n);
			} else {
				_neighbours_map[d->dest()]->sample(d->time());
			}
		} else {
			_num_failed++;
//			auto result = _neighbours_map.find(d->dest());
//			if (result != _neighbours_map.end()) {
//				Neighbour* n = result->second;
//				_neighbours_map.erase(result->first);
//				_neighbours_set.erase(n);
//				delete n;
//			}
		}
//		std::cout << "Returned: "
//			<< _num_succeeded + _num_failed << '/' 
//			<< _num_neighbours
//			<< " from " << d->dest() 
//			<< std::endl;
		
		std::cout << _source << ": result #" << _attempts
			<< ' '
			<< num_succeeded()
			<< '+'
			<< num_failed()
			<< '/'
			<< num_neighbours()
			<< ' '
			<< min_samples()
			<< std::endl;

		if (_num_succeeded + _num_failed == _num_neighbours) {
			
			std::cout << _source << ": end of attempt #" << _attempts << std::endl;

//			if (num_succeeded() == NUM_SERVERS-1 && min_samples() == MIN_SAMPLES) {
			if (min_samples() == MIN_SAMPLES) {
				std::cout << "Neighbours:" << std::endl;
				std::ostream_iterator<Neighbour*> it(std::cout, "\n");
				std::copy(_neighbours_set.cbegin(), _neighbours_set.cend(), it);
				std::set<Neighbour> neighbours;
				std::for_each(_neighbours_set.cbegin(), _neighbours_set.cend(),
					[&neighbours] (const Neighbour* rhs)
				{
					neighbours.insert(*rhs);
				});
//				this->upstream(the_server(), new Candidate(_source, neighbours));
//				commit(the_server());
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


	const Neighbour* leader_candidate() const {
		return *_neighbours_set.cbegin();
	}

private:

	uint32_t min_samples() const {
		return std::accumulate(_neighbours_set.cbegin(), _neighbours_set.cend(),
			std::numeric_limits<uint32_t>::max(),
			[] (uint32_t m, const Neighbour* rhs)
		{
			return std::min(m, rhs->num_samples());
		});
	}

	Neighbour* find_neighbour(Endpoint addr) {
		auto result = _neighbours_map.find(addr);
		return result == _neighbours_map.end() ? nullptr : result->second;
	}

	Endpoint _source;
	Port_range _port_range;

	std::map<Endpoint, Neighbour*> _neighbours_map;
	std::set<Neighbour*, Higher_metric> _neighbours_set;

	uint32_t _num_neighbours = 0;
	uint32_t _num_succeeded = 0;
	uint32_t _num_failed = 0;

	uint32_t _attempts = 0;

	static const uint32_t MAX_ATTEMPTS = 17;
	static const uint32_t MIN_SAMPLES = 5;
	static const uint32_t MAX_SAMPLES = 5;
};


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc < 2) {
			try {
				std::vector<Endpoint> servers = NEIGHBOURS;
//				std::vector<Endpoint> servers(NUM_SERVERS);
//				for (int i=0; i<NUM_SERVERS; ++i) {
//					servers[i] = Endpoint("127.0.0.1", DEFAULT_PORT + i*PORT_INCREMENT);
//				}
				Port_range port_range(DEFAULT_PORT, DEFAULT_PORT + (NUM_SERVERS-1)*PORT_INCREMENT + 1);
				Process_group processes;
				for (int i=0; i<NUM_SERVERS; ++i) {
					Endpoint endpoint = servers[i];
					processes.add([endpoint, port_range, &argv] () {
//						std::string arg1 = to_string(endpoint);
//						std::string arg2 = to_string(port_range);
//						char* const args[] = {argv[0], (char*)arg1.c_str(), (char*)arg2.c_str(), 0};
//						char* const env[] = { 0 };
//						check("execve()", ::execve(argv[0], args, env));
//						return 0;
						return execute(argv[0], endpoint, port_range);
					});
				}

				for (int i=0; i<NUM_SERVERS; ++i) {
					std::cout << "Forked " << processes[i].id() << std::endl;
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
				std::cout << "Command to view logs:\n" << s.str() << std::endl;
//				::system(s.str().c_str());
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
