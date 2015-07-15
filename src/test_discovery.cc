#include <factory/factory.hh>

using namespace factory;

const Port DISCOVERY_PORT = 10000;

std::vector<Endpoint> all_peers;
std::thread exiter;

uint32_t my_netmask() {
	uint32_t npeers = static_cast<uint32_t>(all_peers.size());
	uint32_t power = factory::log2(npeers);
	uint32_t rem = UINT32_C(1) << power;
	if (rem < npeers) rem <<= 1;
	return std::numeric_limits<uint32_t>::max() - (rem - 1);
}

struct Node {
	explicit Node(const Endpoint& rhs): addr(rhs) {}
	friend std::ostream& operator<<(std::ostream& out, const Node& rhs) {
		return out << "n" << uint64_t(rhs.addr.address()) * uint64_t(rhs.addr.port());
	}
private:
	const Endpoint& addr;
};

struct Edge {
	Edge(const Endpoint& a, const Endpoint& b): x(a), y(b) {}
	friend std::ostream& operator<<(std::ostream& out, const Edge& rhs) {
		return out << Node(rhs.x) << '_' << Node(rhs.y);
	}
private:
	const Endpoint& x;
	const Endpoint& y;
};

struct Peer {

	typedef int64_t Metric;

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
			|| (age() > MAX_AGE && MIN_SAMPLES > 0);
	}

	bool operator<(const Peer& rhs) const {
		return metric() < rhs.metric();
	}

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

	uint32_t num_samples() const { return _num_samples; }
	uint32_t num_errors() const { return _num_errors; }

	Time age() const { return current_time_nano() - _update_time; }
	void update() {
		Time old = _update_time;
		_update_time = current_time_nano();
		if (_update_time - old > MAX_AGE) {
			_num_errors = 0;
		}
	}

	Time _t = 0;
	uint32_t _num_samples = 0;
	uint32_t _num_errors = 0;
	Time _update_time = 0;

	static const uint32_t MAX_SAMPLES = 1000;
	static const uint32_t MIN_SAMPLES = 0;
	static const uint32_t MAX_ERRORS  = 3;

	static const Time MAX_AGE = std::chrono::milliseconds(100000).count();
};

Time prog_start = current_time_nano();

struct Compare_distance {

	explicit Compare_distance(Endpoint from): _from(from) {}

	bool operator()(const std::pair<const Endpoint,Peer>& lhs, const std::pair<const Endpoint,Peer>& rhs) const {
//		Logger<Level::DISCOVERY>() << "hoho" << std::endl;
//		return lhs.second.metric() < rhs.second.metric();
//		return lhs.second.metric() < rhs.second.metric()
//			|| (lhs.second.metric() == rhs.second.metric() && lhs.first < rhs.first);
		
		return std::make_pair(addr_distance(_from, lhs.first), lhs.first)
			< std::make_pair(addr_distance(_from, rhs.first), rhs.first);
	}

	bool operator()(const Endpoint& lhs, const Endpoint& rhs) const {
		return std::make_pair(addr_distance(_from, lhs), lhs)
			< std::make_pair(addr_distance(_from, rhs), rhs);
	}

private:
	constexpr static uint32_t abs_sub(uint32_t a, uint32_t b) {
		return a < b ? b-a : a-b;
	}
	
	constexpr static uint32_t lvl_sub(uint32_t a, uint32_t b) {
		return a > b ? 2000 : (b-a == 0 ? 1000 : b-a);
	}

	static const uint32_t p = 1;
	static const uint32_t fanout = UINT32_C(1) << p;
	
	static std::pair<uint32_t, uint32_t> addr_level_num(Endpoint addr) {
		uint32_t pos = addr.position(my_netmask());
		uint32_t lvl = log(pos, p);
		uint32_t num = pos - (UINT32_C(1) << (lvl*p));
		return std::make_pair(lvl, num);
	}
	
	static std::pair<uint32_t, uint32_t> addr_distance(Endpoint lhs, Endpoint rhs) {
		auto p1 = addr_level_num(lhs);
		auto p2 = addr_level_num(rhs);
		return std::make_pair(lvl_sub(p2.first, p1.first),
			abs_sub(p2.second, p1.second/fanout));
	}

	Endpoint _from;
};

struct Peers {

	typedef std::map<Endpoint, Peer> Map;
	typedef std::set<Endpoint> Set;

	explicit Peers(Endpoint addr):
		_peers(),
		_this_addr(addr),
		_principal(),
		_subordinates()
	{}

	Peers(const Peers& rhs):
		_peers(rhs._peers),
		_this_addr(rhs._this_addr),
		_principal(rhs._principal),
		_subordinates(rhs._subordinates) {}

	void add_peer(Endpoint addr) {
		if (!addr || addr == _this_addr) return;
		if (_peers.count(addr) == 0) {
			_peers[addr];
		}
	}

	Endpoint best_peer() const {
		return std::min_element(_peers.begin(), _peers.end(),
			Compare_distance(_this_addr))->first;
	}

	Endpoint this_addr() const { return _this_addr; }
	Endpoint principal() const { return _principal; }

	void remove_principal() {
		_peers.erase(_principal);
		_principal = Endpoint();
	}

	bool change_principal(Endpoint new_princ) {
		Endpoint old_princ = _principal;
		bool success = false;
		if (old_princ != new_princ) {
			_principal = new_princ;
			add_peer(_principal);
			_peers.erase(old_princ);
			_subordinates.erase(new_princ);
			success = true;
		}
		return success;
	}

	void revert_principal(Endpoint old_princ) {
		Logger<Level::DISCOVERY>() << "Reverting principal to " << old_princ << std::endl;
		_peers.erase(_principal);
		if (old_princ) {
			add_peer(old_princ);
		}
		_principal = old_princ;
		debug();
	}

	void connected_peers(Set& peers) const {
		peers.insert(_subordinates.begin(), _subordinates.end());
		if (_principal) {
			peers.insert(_principal);
		}
	}

	void update_peers(const Peers& rhs) {
		for (const auto& p : rhs._peers) {
			auto it = _peers.find(p.first);
			if (it == _peers.end()) {
				_peers.insert(p);
			} else {
				it->second = p.second;
			}
		}
	}

	void add_subordinate(Endpoint addr) {
		Logger<Level::DISCOVERY>() << "Adding subordinate = " << addr << std::endl;
		if (_subordinates.count(addr) == 0) {
			Logger<Level::GRAPH>()
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "g." << Edge(addr, _this_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(_this_addr) << ')'
				<< "}, "
				<< "undo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, _this_addr) << ")"
				<< "}}"
				<< ";log[logline-1].time=" << current_time_nano() - prog_start << "*1e-6"
				<< std::endl;
		}
		_subordinates.insert(addr);
		add_peer(addr);
	}

	void remove_subordinate(Endpoint addr) {
		Logger<Level::DISCOVERY>() << "Removing subordinate = " << addr << std::endl;
		if (_subordinates.count(addr) > 0) {
			Logger<Level::GRAPH>()
				<< "log[logline++] = {"
				<< "redo: function () {"
				<< "graph.removeEdge(g." << Edge(addr, _this_addr) << ')'
				<< "}, undo: function() {"
				<< "g." << Edge(addr, _this_addr) << " = graph.newEdge("
				<< "g." << Node(addr) << ',' << "g." << Node(_this_addr) << ')'
				<< "}}"
				<< ";log[logline-1].time=" << current_time_nano() - prog_start << "*1e-6"
				<< std::endl;
		}
		_subordinates.erase(addr);
		_peers.erase(addr);
	}

	size_t num_subordinates() const { return _subordinates.size(); }

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

	void debug() {
		Logger<Level::DISCOVERY> log;
		log << "Principal = " << _principal << ", subordinates = ";
		std::ostream_iterator<Endpoint> it(log.ostream(), ", ");
		std::copy(_subordinates.begin(), _subordinates.end(), it);
		log << " peers = ";
		write(log.ostream(), ", ");
		log << std::endl;
	}

private:
	Map _peers;
	Endpoint _this_addr;
	Endpoint _principal;
	Set _subordinates;
};

struct Profiler: public Mobile<Profiler> {

	typedef uint8_t State;

	Profiler(): _peers() {}

	void act() {
		_state = 1;
		commit(remote_server());
	}

	void write_impl(packstream& out) {
		if (_state == 0) {
			_time = current_time_nano();
		}
		out << _state << _time;
		out << uint32_t(_peers.size());
		for (const Endpoint& sub : _peers) {
			out << sub;
		}
	}

	void read_impl(packstream& in) {
		in >> _state >> _time;
		if (_state == 1) {
			_time = current_time_nano() - _time;
		}
		uint32_t n = 0;
		in >> n;
		_peers.clear();
		for (uint32_t i=0; i<n; ++i) {
			Endpoint addr;
			in >> addr;
			_peers.insert(addr);
		}
	}

	void copy_peers_from(const Peers& peers) {
		peers.connected_peers(_peers);
	}

	void copy_peers_to(Peers& peers) {
		for (const Endpoint& addr : _peers) {
			peers.add_peer(addr);
		}
	}

//	uint32_t num_peers() const { return _peers.size(); }

	Time time() const { return _time; }
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:

	Time _time = 0;
	State _state = 0;

	std::set<Endpoint> _peers;
};


struct Ping: public Mobile<Ping> {

	Ping() {}

	void act() { commit(remote_server()); }

	void write_impl(packstream&) { }
	void read_impl(packstream&) { }

	static void init_type(Type* t) {
		t->id(7);
	}
};

struct Scanner: public Identifiable<Kernel> {

	explicit Scanner(Endpoint addr, Endpoint st_addr):
		_source(addr),
		_oldaddr(st_addr),
		_scan_addr(),
		_servers(create_servers(all_peers)),
		_discovered_node()
		{}

	void act() {
		if (_servers.empty()) {
//			Logger<Level::DISCOVERY>() << "There are no servers to scan." << std::endl;
			commit(the_server(), Result::USER_ERROR);
		} else {
			if (!_scan_addr) {
				if (_oldaddr) {
					_scan_addr = _oldaddr;
					_scan_addr = next_scan_addr();
				} else {
					_scan_addr = start_addr(all_peers);
				}
			}
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
	Endpoint scan_addr() const { return _scan_addr; }

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

	Endpoint start_addr(const std::vector<Endpoint>& peers) const {
		if (peers.empty()) return Endpoint();
		auto it = std::min_element(peers.begin(), peers.end(), Compare_distance(_source));
		if (*it != _source) {
			return *it;
		}
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
		Logger<Level::DISCOVERY>() << "scanning " << ping->to() << std::endl;
		remote_server()->send(ping);
	}

	Endpoint _source;
	Endpoint _oldaddr;
	Endpoint _scan_addr;
	std::vector<Endpoint> _servers;
	Endpoint _discovered_node;

	uint32_t _num_scanned = 0;
};

struct Discoverer: public Identifiable<Kernel> {

	explicit Discoverer(const Peers& p): _peers(p) {}

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
		prof->copy_peers_to(_peers);
		if (--_num_sent == 0) {
			commit(the_server());
		}
	}

	const Peers& peers() const { return _peers; }

private:
	Peers _peers;
	size_t _num_sent = 0;
};

struct Negotiator: public Mobile<Negotiator> {

	Negotiator():
		_old_principal(), _new_principal() {}

	Negotiator(Endpoint old, Endpoint neww):
		_old_principal(old), _new_principal(neww) {}

	void negotiate(Peers& peers) {
		this->principal(this->parent());
		this->result(Result::SUCCESS);
		Endpoint this_addr = peers.this_addr();
		if (_new_principal == this_addr) {
			// principal becomes subordinate
			if (this->from() == peers.principal()) {
				if (_old_principal) {
					peers.remove_principal();
				} else {
					// root tries to swap with its subordinate
					this->result(Result::USER_ERROR);
				}
			}
			if (this->result() != Result::USER_ERROR) {
				peers.add_subordinate(this->from());
			}
		} else
		if (_old_principal == this_addr) {
			// something fancy is going on
			if (this->from() == peers.principal()) {
				this->result(Result::USER_ERROR);
			} else {
				peers.remove_subordinate(this->from());
			}
		}
		_stop = !peers.principal() && peers.num_subordinates() == all_peers.size()-1;
		remote_server()->send(this);
	}
	
	void write_impl(packstream& out) {
		// TODO: if moves_upstream
		out << _old_principal << _new_principal << _stop;
	}

	void read_impl(packstream& in) {
		in >> _old_principal >> _new_principal >> _stop;
	}

	bool stop() const { return _stop; }

	static void init_type(Type* t) {
		t->id(8);
		t->name("Negotiator");
	}

private:
	Endpoint _old_principal;
	Endpoint _new_principal;
	bool _stop = false;
};

struct Master_negotiator: public Identifiable<Kernel> {

	Master_negotiator(Endpoint old, Endpoint neww):
		_old_principal(old), _new_principal(neww) {}
	
	void act() {
		if (_old_principal) {
			send_negotiator(_old_principal);
		}
		send_negotiator(_new_principal);
	}

	void react(Kernel* k) {
		Logger<Level::DISCOVERY>()
			<< "Negotiator returned from " << k->from()
			<< " with result=" << k->result() << std::endl; 
//		if (k->from() == _new_principal && k->result() != Result::SUCCESS) {
		if (this->result() == Result::UNDEFINED && k->result() != Result::SUCCESS) {
			this->result(k->result());
		}
//		Negotiator* neg = dynamic_cast<Negotiator*>(k);
		if (--_num_sent == 0) {
			if (this->result() == Result::UNDEFINED) {
				this->result(Result::SUCCESS);
			}
			this->principal(this->parent());
			the_server()->send(this);
		}
	}

	Endpoint old_principal() const { return _old_principal; }

private:

	void send_negotiator(Endpoint addr) {
		++_num_sent;
		Negotiator* n = new Negotiator(_old_principal, _new_principal);
		n->principal(addr.address());
		n->to(addr);
		upstream(remote_server(), n);
	}

	Endpoint _old_principal;
	Endpoint _new_principal;
	uint8_t _num_sent = 0;
};

struct Master_discoverer: public Identifiable<Kernel> {

	Master_discoverer(const Master_discoverer&) = delete;
	Master_discoverer& operator=(const Master_discoverer&) = delete;

	explicit Master_discoverer(Endpoint this_addr):
		factory::Identifiable<Kernel>(this_addr.address()),
		_peers(this_addr),
		_scanner(nullptr),
		_discoverer(nullptr),
		_negotiator(nullptr)
	{}

	void act() {
		prog_start = current_time_nano();
//		Time start_time = this_process::getenv("START_TIME", Time(0));
//		if (start_time > 0) {
//			using namespace std::chrono;
//			nanoseconds now = duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
//			nanoseconds amount = nanoseconds(start_time) - now;
//			std::clog << "Sleeping for " << amount.count() << "ns" << std::endl;
//			std::this_thread::sleep_for(amount);
//			prog_start = current_time_nano();
//		}
		exiter = std::thread([this] () { 
			std::this_thread::sleep_for(std::chrono::seconds(10));
			Logger<Level::DISCOVERY>() << "Hail the new king! addr="
				<< _peers.this_addr()
				<< ",peers=" << this->_peers
				<< ",npeers=" << all_peers.size() << std::endl;
			_peers.debug();
			__factory.stop();
		});
		Logger<Level::GRAPH>()
			<< "startTime.push("
			<< current_time_nano() - prog_start
			<< "*1e-6); // "
			<< _peers.this_addr()
			<< std::endl;
		if (!all_peers.empty() && _peers.this_addr() != all_peers[0]) {
			run_scan();
		}
	}

	void react(Kernel* k) {
		if (_scanner == k) {
			if (k->result() != Result::SUCCESS) {
//				Time wait_time = this_process::getenv("WAIT_TIME", Time(60000000000UL));
//				if (current_time_nano() - prog_start > wait_time) {
//					Logger<Level::DISCOVERY>() << "Hail the new king "
//						<< _peers.this_addr() << "! npeers = " << all_peers.size() << std::endl;
//					__factory.stop();
//				}
				run_scan(_scanner->discovered_node());
			} else {
				Logger<Level::DISCOVERY>() << "Change 1" << std::endl;
				change_principal(_scanner->discovered_node());
				run_discovery();
				_scanner = nullptr;
			}
		} else 
		if (_discoverer == k) {
			if (k->result() == Result::SUCCESS) {
				Discoverer* dsc = dynamic_cast<Discoverer*>(k);
				_peers.update_peers(dsc->peers());
				_peers.debug();
				Logger<Level::DISCOVERY>() << "Change 2" << std::endl;
				change_principal(_peers.best_peer());
			} else {
				run_discovery();
			}
		} else
		if (_negotiator == k) {
			if (k->result() != Result::SUCCESS) {
				Master_negotiator* neg = dynamic_cast<Master_negotiator*>(k);
				Endpoint princ = _peers.principal();
				_peers.revert_principal(neg->old_principal());
				run_negotiator(princ, _peers.principal());
			}
			_peers.debug();
			_negotiator = nullptr;
		} else
		if (k->type()) {
			if (k->type()->id() == 2) {
				Profiler* prof = dynamic_cast<Profiler*>(k);
				prof->copy_peers_from(_peers);
				prof->act();
			} else if (k->type()->id() == 8) {
				Negotiator* neg = dynamic_cast<Negotiator*>(k);
				neg->negotiate(_peers);
			}
		}
	}

private:

	void run_scan(Endpoint old_addr = Endpoint()) {
		_scanner = new Scanner(_peers.this_addr(), old_addr);
		if (!old_addr) {
			_peers.add_peer(_scanner->scan_addr());
		}
		upstream(the_server(), _scanner);
	}
	
	void run_discovery() {
		Logger<Level::DISCOVERY>() << "Discovering..." << std::endl;
//		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		upstream(the_server(), _discoverer = new Discoverer(_peers));
	}

	void run_negotiator(Endpoint old_princ, Endpoint new_princ) {
		upstream(the_server(), _negotiator = new Master_negotiator(old_princ, new_princ));
	}

	void change_principal(Endpoint new_princ) {
		Endpoint old_princ = _peers.principal();
		if (!_negotiator && _peers.change_principal(new_princ)) {
			Logger<Level::DISCOVERY>() << "Changing principal to " << new_princ << std::endl;
			_peers.debug();
			run_negotiator(old_princ, new_princ);
		}
	}

	std::string cache_filename() const {
		std::stringstream s;
		s << "/tmp/";
		s << _peers.this_addr() << ".cache";
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

	Peers _peers;

	Scanner* _scanner;
	Discoverer* _discoverer;
	Master_negotiator* _negotiator;

	static const uint32_t MIN_SAMPLES = 7;
};

uint32_t num_peers() {
	std::stringstream s;
	s << ::getenv("NUM_PEERS");
	uint32_t n = 0;
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
	for (Endpoint addr : all_peers) {
		auto it = std::min_element(all_peers.begin(), all_peers.end(),
			Compare_distance(addr));
		Logger<Level::DISCOVERY>()
			<< "Best link: " << addr << " -> " << *it << std::endl;
	}
//	uint32_t p = 1;
//	uint32_t fanout = 1 << p;
//	for (Endpoint addr : all_peers) {
//		uint32_t pos = addr.position(my_netmask());
//		uint32_t lvl = log(pos, p);
//		uint32_t rem = pos - (1 << lvl);
//		auto dist = addr_distance(all_peers[7], addr);
//		Logger<Level::DISCOVERY>()
//			<< "Netmask = "
//			<< addr << ", "
//			<< Endpoint(my_netmask(), 0) << ", "
//			<< pos << ", "
//			<< lvl << ":" << rem << " --> "
//			<< lvl-1 << ":" << rem/fanout << ", distance = "
//			<< dist.first << ',' << dist.second
//			<< std::endl;
//	}
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

void write_graph_nodes() {
	for (Endpoint addr : all_peers) {
		Logger<Level::GRAPH>()
			<< "log[logline++] = {"
			<< "redo: function() { g." << Node(addr) << " = graph.newNode({label:'" << addr << "'}) }, "
			<< "undo: function() { graph.removeNode(g." << Node(addr) << ")}}"
			<< ";log[logline-1].time=" << current_time_nano() - prog_start << "*1e-6"
			<< std::endl;
	}
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 2) {
			try {
				uint32_t npeers = num_peers();
				std::string base_ip = argc == 2 ? argv[1] : "127.0.0.1"; 
				generate_all_peers(npeers, base_ip);
				if (write_cache()) {
					write_cache_all();
					return 0;
				}

				Process_group processes;
				int start_id = 1000;
				for (Endpoint endpoint : all_peers) {
					processes.add([endpoint, &argv, start_id, npeers, &base_ip] () {
						this_process::env("START_ID", start_id);
						return this_process::execute(argv[0],
							"--bind-addr", endpoint,
							"--num-peers", npeers,
							"--base-ip", base_ip);
					});
					start_id += 1000;
				}

				Logger<Level::DISCOVERY>() << "Forked " << processes << std::endl;
				
				retval = processes.wait();

			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		} else {
			uint32_t npeers = 3;
			Endpoint bind_addr(get_bind_address(), DISCOVERY_PORT);
			std::string base_ip = "127.0.0.1";
			Command_line cmdline(argc, argv);
			cmdline.parse([&bind_addr, &npeers, &base_ip](const std::string& arg, std::istream& in) {
				     if (arg == "--bind-addr") { in >> bind_addr; }
				else if (arg == "--num-peers") { in >> npeers; }
				else if (arg == "--base-ip")   { in >> base_ip; }
			});
			Logger<Level::DISCOVERY>() << "Bind address " << bind_addr << std::endl;
			generate_all_peers(npeers, base_ip);
			if (Endpoint(base_ip,0).address() == bind_addr.address()) {
				write_graph_nodes();
			}
			try {
				the_server()->add_cpu(0);
				remote_server()->socket(bind_addr);
				__factory.start();
				Time start_delay = this_process::getenv("START_DELAY", Time(bind_addr == Endpoint("127.0.0.1", 10000) ? 0 : 2));
				Master_discoverer* master = new Master_discoverer(bind_addr);
				master->after(std::chrono::seconds(start_delay));
//				master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
				timer_server()->send(master);
				__factory.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
