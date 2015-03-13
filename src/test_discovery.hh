#include "process.hh"

using namespace factory;

typedef Interval<Port> Port_range;

const Port DISCOVERY_PORT = 10000;

std::vector<Endpoint> NEIGHBOURS = {
	Endpoint("127.0.0.1", DISCOVERY_PORT),
	Endpoint("127.0.0.2", DISCOVERY_PORT),
	Endpoint("127.0.0.3", DISCOVERY_PORT)
};

struct Discovery: public Identifiable<Mobile<Discovery>> {

	typedef uint64_t Time;
	typedef uint8_t State;

	Discovery() {}

	void act() {
		commit(remote_server());
	}

	void write_impl(Foreign_stream& out) {
		if (_state == 0) {
			_time = current_time();
		}
		_state++;
		out << _state << _time;
	}

	void read_impl(Foreign_stream& in) {
		in >> _state >> _time;
		_state++;
		if (_state == 3) {
			_time = current_time() - _time;
		}
	}

	Time time() const { return _time; }

	static Time current_time() {
		return std::chrono::steady_clock::now().time_since_epoch().count();
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:
	Time _time = 0;
	State _state = 0;
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
	bool no_vote() const { return _vote == 2; }
	
	static void init_type(Type* t) {
		t->id(3);
	}

	void write_impl(Foreign_stream& out) { out << _vote << _src; }
	void read_impl(Foreign_stream& in) { in >> _vote >> _src; }

private:
	Endpoint _src;
	uint8_t _vote = 2;
};

struct Candidate: public Identifiable<Kernel> {
	
	Candidate(Endpoint src, const std::set<Neighbour>& nb):
		factory::Identifiable<Kernel>(2),
		_source(src),
		_neighbours(nb) {}
	
	void act() {
		_num_for = 0;
		_num_against = 0;
		_num_neutral = 0;
		Logger(Level::KERNEL) << "Starting poll " << _source << std::endl;
		std::for_each(_neighbours.cbegin(), _neighbours.cend(),
			[this] (const Neighbour& rhs)
		{
			Ballot* k = new Ballot;
			k->parent(this);
			k->principal(this); // the same id
			k->source(_source);
			Logger(Level::KERNEL)
				<< _source
				<< ": polling "
				<< rhs.addr()
				<< ", kernel id = "
				<< k->id()
				<< std::endl;
			remote_server()->send(k, rhs.addr());
		});
	}

	void react(factory::Kernel* k) {
		Ballot* b = dynamic_cast<Ballot*>(k);
		if (b->result() == Result::NO_PRINCIPAL_FOUND) {
			Logger(Level::KERNEL)
				<< _source
				<< ": poll error "
				<< " from = " << b->source()
				<< std::endl;
		}
		// ballot from another host
		if (b->source() != _source) {
			Logger(Level::KERNEL) << _source << ": poll vote " << std::endl;
			// the address of the neighbour with the highest rank is the same
			// as the address of the candidate
			Ballot* bb = new Ballot;
			bb->parent(this);
			bb->principal(this);
			bb->from(b->from());
			bb->source(b->source());
			bb->vote(b->source() == _neighbours.cbegin()->addr());
			bb->result(Result::SUCCESS);
			remote_server()->send(bb, b->from());
		} else {
			// returning ballot
			if (b->result() == Result::SUCCESS) {
				if (b->vote()) _num_for++; else _num_against++;
			} else {
				_num_neutral++;
			}
			// preliminary results
			Logger(Level::KERNEL) << _source << " preliminary: "
				<< _num_for << '+'
				<< _num_against << '+'
				<< _num_neutral << '/'
				<< _neighbours.size()
				<< std::endl;
			// all ballots have been returned
			if (_num_for + _num_against + _num_neutral == _neighbours.size()) {
				Logger(Level::KERNEL) << _source << " exit poll: "
					<< _num_for << '+'
					<< _num_against << '+'
					<< _num_neutral << '/'
					<< _neighbours.size()
					<< std::endl;
//				commit(the_server());
			}
		}
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
//				Logger(Level::KERNEL) << ep << std::endl;
//				Discovery_kernel* k = new Discovery_kernel;
//				k->parent(this);
//				remote_server()->send(k, ep);
//			}
//		});
//	}

	Discoverer(Endpoint endpoint):
		factory::Identifiable<Kernel>(1),
		_source(endpoint),
		_servers(create_servers(NEIGHBOURS)) {}
	
	std::vector<Endpoint> create_servers(std::vector<Endpoint> servers) {
		std::vector<Endpoint> tmp;
		std::copy_if(servers.cbegin(), servers.cend(),
			std::back_inserter(tmp), [this] (Endpoint addr) {
				return addr != this->_source;
			});
		return tmp;
	}

	void act() {
		_attempts++;
		_num_succeeded = 0;
		_num_failed = 0;
		_num_neighbours = 0;

		Logger(Level::KERNEL) << _source << ": attempt #" << _attempts << std::endl;

		std::vector<Discovery*> kernels;


		std::for_each(_servers.cbegin(), _servers.cend(),
			[this, &kernels] (const Endpoint& ep)
		{
			Neighbour* n = find_neighbour(ep);
			Logger(Level::KERNEL) 
				<< _source
				<< ": sending to " << ep << ' '
				<< ((n != nullptr) ? n->num_samples() : 0) << std::endl;
			if (n == nullptr || n->num_samples() < MIN_SAMPLES) {
				Logger(Level::KERNEL) << "Sending to " << ep << std::endl;
				Discovery* k = new Discovery;
				k->parent(this);
				k->to(ep);
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
//				Logger(Level::KERNEL) 
//					<< _source
//					<< ": sending to " << ep << ' '
//					<< ((n != nullptr) ? n->num_samples() : 0) << std::endl;
//				if (n == nullptr || n->num_samples() < MIN_SAMPLES) {
//					Logger(Level::KERNEL) << "Sending to " << ep << std::endl;
//					Discovery* k = new Discovery;
//					k->parent(this);
//					k->dest(ep);
//					k->source(_source);
//					kernels.push_back(k);
//				}
//			}
//		});

		// send discovery messages
		Logger(Level::KERNEL) << "Sending discovery messages " << ::getpid() << ", " << _source << std::endl;
		_num_neighbours = kernels.size();
		for (Discovery* d : kernels) {
			remote_server()->send(d, d->to());
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
			Logger(Level::KERNEL) << _source << ": success for " << d->from() << std::endl;
			_num_succeeded++;
			if (_neighbours_map.find(d->from()) == _neighbours_map.end()) {
				Neighbour* n = new Neighbour(d->from(), d->time());
				_neighbours_map[d->from()] = n;
				_neighbours_set.insert(n);
			} else {
				_neighbours_map[d->from()]->sample(d->time());
			}
		} else {
			_num_failed++;
//			auto result = _neighbours_map.find(d->from());
//			if (result != _neighbours_map.end()) {
//				Neighbour* n = result->second;
//				_neighbours_map.erase(result->first);
//				_neighbours_set.erase(n);
//				delete n;
//			}
		}
//		Logger(Level::KERNEL) << "Returned: "
//			<< _num_succeeded + _num_failed << '/' 
//			<< _num_neighbours
//			<< " from " << d->from() 
//			<< std::endl;
		
		Logger(Level::KERNEL) << _source << ": result #" << _attempts
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
			
			Logger(Level::KERNEL) << _source << ": end of attempt #" << _attempts << std::endl;

			if (min_samples() == MIN_SAMPLES) {
				Logger log(Level::KERNEL);
				log << "Neighbours: ";
				std::ostream_iterator<Neighbour*> it(log.ostream(), ", ");
				std::copy(_neighbours_set.cbegin(), _neighbours_set.cend(), it);
				std::set<Neighbour> neighbours;
				std::for_each(_neighbours_set.cbegin(), _neighbours_set.cend(),
					[&neighbours] (const Neighbour* rhs)
				{
					neighbours.insert(*rhs);
				});
				log << std::endl;
				this->upstream(the_server(), new Candidate(_source, neighbours));
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

	std::map<Endpoint, Neighbour*> _neighbours_map;
	std::set<Neighbour*, Higher_metric> _neighbours_set;
	std::vector<Endpoint> _servers;

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
				Process_group processes;
				int start_id = 1000;
				for (Endpoint endpoint : NEIGHBOURS) {
					processes.add([endpoint, &argv, start_id] () {
						return execute(start_id, argv[0], endpoint);
					});
					start_id += 1000;
				}

				Logger(Level::KERNEL) << "Forked " << processes << std::endl;
				
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
				Logger(Level::KERNEL) << "Command to view logs:\n" << s.str() << std::endl;
//				::system(s.str().c_str());
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		} else {
			std::stringstream cmdline;
			for (int i=1; i<argc; ++i) cmdline << argv[i] << ' ';
			Endpoint endpoint;
			cmdline >> endpoint;
			try {
				the_server()->add(0);
				the_server()->start();
				remote_server()->socket(endpoint);
				remote_server()->start();
				the_server()->send(new Discoverer(endpoint));
				__factory.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};
