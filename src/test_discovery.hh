#include "process.hh"

using namespace factory;

const Port DISCOVERY_PORT = 10000;

std::vector<Endpoint> all_peers;

struct Peer {

	typedef uint64_t Time;
	typedef uint32_t Metric;

	Peer() {}
	Peer(const Peer& rhs):
		_t(rhs._t),
		_num_samples(rhs._num_samples),
		_num_errors(rhs._num_errors),
		_update_time(rhs._update_time) {}

	Metric metric() const { return _t/1000/1000/1000; }

	void collect_sample(Time rhs) {
		update();
		if (++_num_samples == MAX_SAMPLES) {
			_num_samples = 1;
			_t = 0;
		}
		_t = ((_num_samples-1)*_t + rhs) / _num_samples;
	}

	void collect_error(uint32_t cnt) {
		update();
		_num_errors += cnt;
	}

	bool needs_update() const {
		return (num_samples() < MIN_SAMPLES && num_errors() < MAX_ERRORS)
			|| age() > MAX_AGE;
	}

	bool operator<(const Peer& rhs) const {
		return metric() > rhs.metric();
	}

//	friend bool operator<(const std::pair<Endpoint,Peer>& lhs, const std::pair<Endpoint,Peer>& rhs) {
////		return lhs.second.metric() < rhs.second.metric()
////			|| (lhs.second.metric() == rhs.second.metric() && lhs.first > rhs.first);
//		return lhs.first < rhs.first;
//	}

	Peer& operator=(const Peer& rhs) {
		_t = rhs._t;
		_num_samples = rhs._num_samples;
		_num_errors = rhs._num_errors;
		_update_time = rhs._update_time;
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& out, const Peer& rhs) {
		return out
			<< rhs._t << ' '
			<< rhs._num_samples << ' '
			<< rhs._num_errors << ' '
			<< rhs._update_time;
	}

	friend std::istream& operator>>(std::istream& in, Peer& rhs) {
		return in
			>> rhs._t
			>> rhs._num_samples
			>> rhs._num_errors
			>> rhs._update_time;
	}

	friend std::ostream& operator<<(std::ostream& out, const std::pair<Endpoint,Peer>& rhs) {
		return out
			<< rhs.first << ' '
			<< rhs.second._t << ' '
			<< rhs.second._num_samples << ' '
			<< rhs.second._num_errors << ' '
			<< rhs.second._update_time;
	}

	friend std::istream& operator>>(std::istream& in, std::pair<Endpoint,Peer>& rhs) {
		return in
			>> rhs.first
			>> rhs.second._t
			>> rhs.second._num_samples
			>> rhs.second._num_errors
			>> rhs.second._update_time;
	}

private:

	typedef std::chrono::steady_clock Clock;

	static Time now() {
		return Clock::now().time_since_epoch().count();
	}

	uint32_t num_samples() const { return _num_samples; }
	uint32_t num_errors() const { return _num_errors; }

	Time age() const { return now() - _update_time; }
	void update() {
		Time old = _update_time;
		_update_time = now();
		if (_update_time - old > MAX_AGE) {
			_num_errors = 0;
		}
	}

	Time _t = 0;
	uint32_t _num_samples = 0;
	uint32_t _num_errors = 0;
	Time _update_time = 0;

	static const uint32_t MAX_SAMPLES = 1000;
	static const uint32_t MIN_SAMPLES = 7;
	static const uint32_t MAX_ERRORS  = 3;

	static const Time MAX_AGE = std::chrono::milliseconds(1000).count();
};

struct Peers {

	typedef std::map<Endpoint, Peer> Map;

	Peer* find(Endpoint addr) {
		auto result = _peers.find(addr);
		return result == _peers.end() ? nullptr : &result->second;
	}

	void add_unique(Endpoint addr) {
		auto res = _peers.find(addr);
		if (res == _peers.end()) {
			_peers[addr];
		}
	}
	
	Endpoint best_peer() {
		return std::min_element(_peers.begin(), _peers.end())->first;
	}

	Map::iterator begin() { return _peers.begin(); }
	Map::iterator end() { return _peers.end(); }
	Map::const_iterator begin() const { return _peers.begin(); }
	Map::const_iterator end() const { return _peers.end(); }

	Peer& operator[](const Endpoint& rhs) { return _peers[rhs]; }

	friend std::ostream& operator<<(std::ostream& out, const Peers& rhs) {
		rhs.write(out, "\n");
		return out;
	}

	friend std::istream& operator>>(std::istream& in, Peers& rhs) {
		Endpoint addr;
		Peer p;
		while (in >> addr >> p) {
			in.get();
			rhs._peers[addr] = p;
		}
		return in;
	}

	void write(std::ostream& out, const char* sep = "\n") const {
		std::ostream_iterator<std::pair<Endpoint,Peer>> it(out, sep);
		std::copy(_peers.begin(), _peers.end(), it);
	}

private:
	Map _peers;
};

struct Profiler: public Mobile<Profiler> {

	typedef Peer::Time Time;
	typedef uint8_t State;
	typedef std::chrono::steady_clock Clock;

	Profiler() {}

	void act() {
		_state = 1;
		commit(remote_server());
	}

	void write_impl(Foreign_stream& out) {
		if (_state == 0) {
			_time = current_time();
		}
		out << _state << _time;
		out << _principal;
		out << uint32_t(_subordinates.size());
		for (Endpoint& sub : _subordinates) {
			out << sub;
		}
	}

	void read_impl(Foreign_stream& in) {
		in >> _state >> _time;
		if (_state == 1) {
			_time = current_time() - _time;
		}
		in >> _principal;
		uint32_t n = 0;
		in >> n;
		_subordinates.resize(n);
		for (uint32_t i=0; i<n; ++i) {
			in >> _subordinates[i];
		}
	}

	void peers(Endpoint princ, const std::vector<Endpoint>& subs) {
		_principal = princ;
		_subordinates = subs;
	}

	void add_peers(Peers& peers) {
		if (_principal) {
			peers.add_unique(_principal);
		}
		for (Endpoint& addr : _subordinates) {
			peers.add_unique(addr);
		}
	}

	Time time() const { return _time*0 + 123; }

	static Time current_time() {
		return Clock::now().time_since_epoch().count();
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:

	Time _time = 0;
	State _state = 0;

	Endpoint _principal;
	std::vector<Endpoint> _subordinates;
};


struct Ping: public Mobile<Ping> {

	Ping() {}

	void act() { commit(remote_server()); }

	void write_impl(Foreign_stream&) { }
	void read_impl(Foreign_stream&) { }

	static void init_type(Type* t) {
		t->id(7);
	}
};

struct Scanner: public Identifiable<Kernel> {

	explicit Scanner(Endpoint addr):
		_source(addr),
		_scan_addr(start_addr(all_peers)),
		_servers(create_servers(all_peers))
		{}

	void act() {
		if (_servers.empty()) {
			Logger(Level::DISCOVERY) << "There are no servers to scan." << std::endl;
			commit(the_server(), Result::USER_ERROR);
		} else {
			_scan_addr = next_scan_addr();
			try_to_connect(_scan_addr);
		}
	}

	void react(Kernel* k) {
		++_num_scanned;
		if (_num_scanned == _servers.size()) {
			_num_scanned = 0;
		}
		if (k->result() != Result::SUCCESS) {
			// continue scanning network
			act();
		} else {
			// the scan is complete
			_discovered_node = _scan_addr;
			commit(the_server());
		}
	}

	Endpoint discovered_node() const { return _discovered_node; }

private:

	std::vector<Endpoint> create_servers(std::vector<Endpoint> servers) {
		std::vector<Endpoint> tmp;
		std::copy_if(servers.cbegin(), servers.cend(),
			std::back_inserter(tmp), [this] (Endpoint addr) {
				return addr < this->_source;
			});
		return tmp;
	}

	/// determine addr to check next
	Endpoint next_scan_addr() {
		auto res = find(_servers.begin(), _servers.end(), _scan_addr);
		return (res == _servers.end() || res == _servers.begin()) 
			? _servers.back()
			: *--res;
	}

	Endpoint start_addr(const std::vector<Endpoint>& peers) {
		if (peers.empty()) return Endpoint();
		auto res = find(peers.begin(), peers.end(), _source);
		Endpoint st = peers.front();
		if (res != peers.end()) {
			st = *res;
		}
		return st;
	}
		
	void try_to_connect(Endpoint addr) {
		Ping* ping = new Ping;
		ping->to(addr);
		ping->parent(this);
		Logger(Level::DISCOVERY) << "scanning " << ping->to() << std::endl;
		remote_server()->send(ping);
	}

	Endpoint _source;
	Endpoint _scan_addr;
	std::vector<Endpoint> _servers;
	Endpoint _discovered_node;

	uint32_t _num_scanned = 0;
};

struct Discoverer: public Identifiable<Kernel> {

	explicit Discoverer(Peers& peers): _peers(peers) {}

	void act() {
		std::vector<Profiler*> profs;
		for (auto& pair : _peers) {
			if (pair.second.needs_update()) {
				Profiler* prof = new Profiler;
				prof->to(pair.first);
				prof->principal(pair.first.address());
				profs.push_back(prof);
			}
		}
		if (profs.empty()) {
			commit(the_server());
		} else {
			_num_sent += profs.size();
			for (Profiler* prof : profs) {
				upstream(remote_server(), prof);
			}
		}
	}

	void react(Kernel* k) {
		Profiler* prof = dynamic_cast<Profiler*>(k);
		Peer& p = _peers[prof->from()];
		if (k->result() != Result::SUCCESS) {
			p.collect_error(1);
		} else {
			p.collect_sample(prof->time());
		}
		if (p.needs_update()) {
			Profiler* prof2 = new Profiler;
			prof2->to(prof->from());
			++_num_sent;
			upstream(remote_server(), prof2);
		}
		prof->add_peers(_peers);
		if (--_num_sent == 0) {
			commit(the_server());
		}
	}

private:
	Peers& _peers;
	uint32_t _num_sent = 0;
};

struct Master_discoverer: public Identifiable<Kernel> {

	explicit Master_discoverer(Endpoint endpoint):
		factory::Identifiable<Kernel>(endpoint.address()),
		_source(endpoint),
		_principal(),
		_subordinates(),
		_peers(),
		_scanner(nullptr),
		_discoverer(nullptr)
	{}

	void act() {
		run_scan();
	}

	void react(Kernel* k) {
		if (_scanner == k) {
			if (k->result() != Result::SUCCESS) {
				run_scan();
			} else {
				change_principal(_scanner->discovered_node());
				run_discovery();
				_scanner = nullptr;
			}
		} else 
		if (_discoverer == k) {
			if (k->result() == Result::SUCCESS) {
				change_principal(_peers.best_peer());
				Logger log(Level::DISCOVERY);
				log << "Peers: ";
				_peers.write(log.ostream(), ", ");
				log << std::endl;
			}
			run_discovery();
		} else {
			Profiler* prof = dynamic_cast<Profiler*>(k);
			prof->peers(_principal, _subordinates);
			prof->act();
		}
	}

private:

	void run_scan() {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		upstream(the_server(), _scanner = new Scanner(_source));
	}
	
	void run_discovery() {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		upstream(the_server(), _discoverer = new Discoverer(_peers));
	}

	void change_principal(Endpoint princ) {
		Logger(Level::DISCOVERY) << "New principal = " << princ << std::endl;
		Endpoint old_princ = _principal;
		_principal = princ;
		_peers.add_unique(_principal);
//		if (old_princ != _principal) {
//			run_discovery();
//		}
		debug();
	}

	void debug() {
		Logger log(Level::DISCOVERY);
		log << "Principal = " << _principal << ", subordinates = ";
		std::ostream_iterator<Endpoint> it(log.ostream(), ",");
		std::copy(_subordinates.begin(), _subordinates.end(), it);
		log << std::endl;
	}

	static void sort_peers(const std::map<Endpoint,Peer>& peers, std::set<Peer>& y) {
		y.clear();
		std::transform(peers.begin(), peers.end(), std::inserter(y, y.begin()), get_peer);
	}

	static Peer get_peer(const std::pair<Endpoint,Peer>& rhs) {
		return std::get<1>(rhs);
	}

	std::string cache_filename() const {
		std::stringstream s;
		s << "/tmp/";
		s << _source << ".cache";
		return s.str();
	}

	bool read_cache() {
		std::ifstream in(cache_filename());
		bool success = in.is_open();
		if (success) {
			in >> _peers;
		}
		return success;
	}

	void write_cache() {
		std::ofstream out(cache_filename());
		out << _peers;
	}

	Endpoint _source;
	Endpoint _principal;
	std::vector<Endpoint> _subordinates;

	Peers _peers;

	Scanner* _scanner;
	Discoverer* _discoverer;

	static const uint32_t MIN_SAMPLES = 7;
};

int num_peers() {
	std::stringstream s;
	s << ::getenv("NUM_PEERS");
	int n = 0;
	s >> n;
	if (n <= 1) {
		n = 3;
	}
	return n;
}

bool write_cache() { return ::getenv("WRITE_CACHE") != NULL; }

void generate_all_peers(uint32_t npeers, std::string base_ip) {
	all_peers.clear();
	uint32_t start = Endpoint(base_ip.c_str(), 0).address();
	uint32_t end = start + npeers;
	for (uint32_t i=start; i<end; ++i) {
		Endpoint endpoint(i, DISCOVERY_PORT);
		all_peers.push_back(endpoint);
	}
}

std::string cache_filename(Endpoint source) {
	std::stringstream s;
	s << "/tmp/" << source << ".cache";
	return s.str();
}

void write_cache_all() {
	std::map<Endpoint,Peer> peers;
	std::transform(all_peers.begin(), all_peers.end(), std::inserter(peers, peers.begin()),
		[] (Endpoint addr) {
			return std::make_pair(addr, Peer());
		});
	for (Endpoint& addr : all_peers) {
		std::ofstream out(cache_filename(addr));
		std::ostream_iterator<std::pair<Endpoint,Peer>> it(out);
		std::copy(peers.begin(), peers.end(), it);
	}
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 2) {
			try {
				int npeers = num_peers();
				std::string base_ip = argc == 2 ? argv[1] : "127.0.0.1"; 
				generate_all_peers(npeers, base_ip);
				if (write_cache()) {
					write_cache_all();
					return 0;
				}

				Process_group processes;
				int start_id = 1000;
				for (Endpoint endpoint : all_peers) {
//					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					processes.add([endpoint, &argv, start_id, npeers, &base_ip] () {
						Process::env("START_ID", start_id);
						return Process::execute(argv[0],
							"--bind-addr", endpoint,
							"--num-peers", npeers,
							"--base-ip", base_ip);
					});
					start_id += 1000;
				}

				Logger(Level::DISCOVERY) << "Forked " << processes << std::endl;
				
				retval = processes.wait();

			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		} else {
			int npeers = 3;
			Endpoint endpoint(get_bind_address(), DISCOVERY_PORT);
			std::string base_ip = "127.0.0.1";
			Command_line cmdline(argc, argv);
			cmdline.parse([&endpoint, &npeers, &base_ip](const std::string& arg, std::istream& in) {
				     if (arg == "--bind-addr") { in >> endpoint; }
				else if (arg == "--num-peers") { in >> npeers; }
				else if (arg == "--base-ip")   { in >> base_ip; }
			});
			Logger(Level::DISCOVERY) << "Bind address " << endpoint << std::endl;
			generate_all_peers(npeers, base_ip);
			try {
				the_server()->add(0);
				the_server()->start();
				remote_server()->socket(endpoint);
				remote_server()->start();
				the_server()->send(new Master_discoverer(endpoint));
				__factory.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};
