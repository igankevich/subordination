#include <fstream>

#include <sysx/socket.hh>
#include <sysx/cmdline.hh>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

#include "discovery.hh"
#include "cache_guard.hh"
#include "springy_graph_generator.hh"
#include "test.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::integral_constant<bool, true> {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::integral_constant<bool, true> {};

}

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sysx::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;

typedef std::chrono::nanoseconds::rep Time;
Time
current_time_nano() {
	using namespace std::chrono;
	typedef std::chrono::steady_clock Clock;
	return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
}

const sysx::port_type DISCOVERY_PORT = 10000;

std::vector<sysx::endpoint> all_peers;

uint32_t my_netmask() {
	uint32_t npeers = static_cast<uint32_t>(all_peers.size());
	uint32_t power = factory::components::log2(npeers);
	uint32_t rem = UINT32_C(1) << power;
	if (rem < npeers) rem <<= 1;
	return std::numeric_limits<uint32_t>::max() - (rem - 1);
}

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

	friend std::ostream& operator<<(std::ostream& out, const std::pair<sysx::endpoint,Peer>& rhs) {
		return out
			<< rhs.first << ' '
			<< rhs.second._t << ' '
			<< rhs.second._num_samples << ' '
			<< rhs.second._num_errors << ' '
			<< rhs.second._update_time;
	}

	friend std::istream& operator>>(std::istream& in, std::pair<sysx::endpoint,Peer>& rhs) {
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

	typedef stdx::log<Compare_distance> this_log;

	explicit Compare_distance(sysx::endpoint from): _from(from) {}

	bool operator()(const std::pair<const sysx::endpoint,Peer>& lhs, const std::pair<const sysx::endpoint,Peer>& rhs) const {
//		this_log() << "hoho" << std::endl;
//		return lhs.second.metric() < rhs.second.metric();
//		return lhs.second.metric() < rhs.second.metric()
//			|| (lhs.second.metric() == rhs.second.metric() && lhs.first < rhs.first);

		return std::make_pair(addr_distance(_from, lhs.first), lhs.first)
			< std::make_pair(addr_distance(_from, rhs.first), rhs.first);
	}

	bool operator()(const sysx::endpoint& lhs, const sysx::endpoint& rhs) const {
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

	static std::pair<uint32_t, uint32_t> addr_level_num(sysx::endpoint addr) {
		uint32_t pos = addr.position(my_netmask());
		uint32_t lvl = factory::components::log(pos, p);
		uint32_t num = pos - (UINT32_C(1) << (lvl*p));
		return std::make_pair(lvl, num);
	}

	static std::pair<uint32_t, uint32_t> addr_distance(sysx::endpoint lhs, sysx::endpoint rhs) {
		auto p1 = addr_level_num(lhs);
		auto p2 = addr_level_num(rhs);
		return std::make_pair(lvl_sub(p2.first, p1.first),
			abs_sub(p2.second, p1.second/fanout));
	}

	sysx::endpoint _from;
};

springy::Springy_graph graph;

struct Peers {

	typedef std::map<sysx::endpoint, Peer> Map;
	typedef std::set<sysx::endpoint> Set;
	typedef stdx::log<Peers> this_log;

	explicit Peers(sysx::endpoint addr):
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

	void add_peer(sysx::endpoint addr) {
		if (!addr || addr == _this_addr) return;
		if (_peers.count(addr) == 0) {
			_peers[addr];
		}
	}

	sysx::endpoint best_peer() const {
		return std::min_element(_peers.begin(), _peers.end(),
			Compare_distance(_this_addr))->first;
	}

	sysx::endpoint this_addr() const { return _this_addr; }
	sysx::endpoint principal() const { return _principal; }

	void remove_principal() {
		_peers.erase(_principal);
		_principal = sysx::endpoint();
	}

	bool change_principal(sysx::endpoint new_princ) {
		sysx::endpoint old_princ = _principal;
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

	void revert_principal(sysx::endpoint old_princ) {
		this_log() << "Reverting principal to " << old_princ << std::endl;
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

	void add_subordinate(sysx::endpoint addr) {
		this_log() << "Adding subordinate = " << addr << std::endl;
		if (_subordinates.count(addr) == 0) {
			graph.add_edge(addr, _this_addr);
		}
		_subordinates.insert(addr);
		add_peer(addr);
	}

	void remove_subordinate(sysx::endpoint addr) {
		this_log() << "Removing subordinate = " << addr << std::endl;
		if (_subordinates.count(addr) > 0) {
			graph.remove_edge(addr, _this_addr);
		}
		_subordinates.erase(addr);
		_peers.erase(addr);
	}

	size_t num_subordinates() const { return _subordinates.size(); }

	Map::iterator begin() { return _peers.begin(); }
	Map::iterator end() { return _peers.end(); }
	Map::const_iterator begin() const { return _peers.begin(); }
	Map::const_iterator end() const { return _peers.end(); }

	Peer& operator[](const sysx::endpoint& rhs) { return _peers[rhs]; }

	friend std::ostream& operator<<(std::ostream& out, const Peers& rhs) {
		rhs.write(out, "\n");
		return out;
	}

	friend std::istream& operator>>(std::istream& in, Peers& rhs) {
		sysx::endpoint addr;
		Peer p;
		while (in >> addr >> p) {
			in.get();
			rhs._peers[addr] = p;
		}
		return in;
	}

	void write(std::ostream& out, const char* sep = "\n") const {
		std::ostream_iterator<std::pair<sysx::endpoint,Peer>> it(out, sep);
		std::copy(_peers.begin(), _peers.end(), it);
	}

	void debug() {
		std::ostream& log = std::clog;
		log << "Principal = " << _principal << ", subordinates = ";
		std::ostream_iterator<sysx::endpoint> it(log, ", ");
		std::copy(_subordinates.begin(), _subordinates.end(), it);
		log << " peers = ";
		write(log, ", ");
		log << std::endl;
	}

private:
	Map _peers;
	sysx::endpoint _this_addr;
	sysx::endpoint _principal;
	Set _subordinates;
};

struct Profiler: public Kernel, public Identifiable_tag {

	typedef uint8_t State;

	Profiler(): _peers() {}

	void act(Server& this_server) override {
		_state = 1;
		commit(this_server.remote_server());
	}

	void write(sysx::packetstream& out) {
		Kernel::write(out);
		if (_state == 0) {
			_time = current_time_nano();
		}
		out << _state << _time;
		out << uint32_t(_peers.size());
		for (const sysx::endpoint& sub : _peers) {
			out << sub;
		}
	}

	void read(sysx::packetstream& in) {
		Kernel::read(in);
		in >> _state >> _time;
		if (_state == 1) {
			_time = current_time_nano() - _time;
		}
		uint32_t n = 0;
		in >> n;
		_peers.clear();
		for (uint32_t i=0; i<n; ++i) {
			sysx::endpoint addr;
			in >> addr;
			_peers.insert(addr);
		}
	}

	void copy_peers_from(const Peers& peers) {
		peers.connected_peers(_peers);
	}

	void copy_peers_to(Peers& peers) {
		for (const sysx::endpoint& addr : _peers) {
			peers.add_peer(addr);
		}
	}

//	uint32_t num_peers() const { return _peers.size(); }

	Time time() const { return _time; }

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{2,
			"Profiler",
			[] (sysx::packetstream& in) {
				Profiler* k = new Profiler;
				k->read(in);
				return k;
			}
		};
	}

private:

	Time _time = 0;
	State _state = 0;

	std::set<sysx::endpoint> _peers;
};


struct Ping: public Kernel, public Identifiable_tag {

	Ping() {}

	void act(Server& this_server) override { commit(this_server.remote_server()); }

//	void write(sysx::packetstream&) { }
//	void read(sysx::packetstream&) { }

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{7,
			"Ping",
			[] (sysx::packetstream& in) {
				Ping* k = new Ping;
				k->read(in);
				return k;
			}
		};
	}

};

struct Scanner: public Kernel, public Identifiable_tag {

	typedef stdx::log<Scanner> this_log;

	explicit Scanner(sysx::endpoint addr, sysx::endpoint st_addr):
		_source(addr),
		_oldaddr(st_addr),
		_scan_addr(),
		_servers(create_servers(all_peers)),
		_discovered_node()
		{}

	void act(Server& this_server) override {
		if (_servers.empty()) {
//			this_log() << "There are no servers to scan." << std::endl;
			commit(this_server.local_server(), Result::USER_ERROR);
		} else {
			if (!_scan_addr) {
				if (_oldaddr) {
					_scan_addr = _oldaddr;
					_scan_addr = next_scan_addr();
				} else {
					_scan_addr = start_addr(all_peers);
				}
			}
			try_to_connect(this_server, _scan_addr);
		}
	}

	void react(Server& this_server, Kernel* k) override {
		++_num_scanned;
		if (_num_scanned == _servers.size()) {
			_num_scanned = 0;
		}
		if (k->result() != Result::SUCCESS) {
			// continue scanning network
			act(this_server);
		} else {
			// the scan is complete
			_discovered_node = _scan_addr;
			commit(this_server.local_server());
		}
	}

	sysx::endpoint discovered_node() const { return _discovered_node; }
	sysx::endpoint scan_addr() const { return _scan_addr; }

private:

	std::vector<sysx::endpoint> create_servers(std::vector<sysx::endpoint> servers) {
		std::vector<sysx::endpoint> tmp;
		std::copy_if(servers.cbegin(), servers.cend(),
			std::back_inserter(tmp), [this] (sysx::endpoint addr) {
				return addr < this->_source;
			});
		return tmp;
	}

	/// determine addr to check next
	sysx::endpoint next_scan_addr() {
		auto res = find(_servers.begin(), _servers.end(), _scan_addr);
		return (res == _servers.end() || res == _servers.begin())
			? _servers.back()
			: *--res;
	}

	sysx::endpoint start_addr(const std::vector<sysx::endpoint>& peers) const {
		if (peers.empty()) return sysx::endpoint();
		auto it = std::min_element(peers.begin(), peers.end(), Compare_distance(_source));
		if (*it != _source) {
			return *it;
		}
		auto res = find(peers.begin(), peers.end(), _source);
		sysx::endpoint st = peers.front();
		if (res != peers.end()) {
			st = *res;
		}
		return st;
	}

	void try_to_connect(Server& this_server, sysx::endpoint addr) {
		Ping* ping = this_server.factory()->new_kernel<Ping>();
		ping->to(addr);
		ping->parent(this);
		this_log() << "scanning " << ping->to() << std::endl;
		this_server.remote_server()->send(ping);
	}

	sysx::endpoint _source;
	sysx::endpoint _oldaddr;
	sysx::endpoint _scan_addr;
	std::vector<sysx::endpoint> _servers;
	sysx::endpoint _discovered_node;

	uint32_t _num_scanned = 0;
};

struct Discoverer: public Kernel, public Identifiable_tag {

	typedef stdx::log<Discoverer> this_log;

	explicit Discoverer(const Peers& p): _peers(p) {}

	void act(Server& this_server) override {
		std::vector<Profiler*> profs;
		for (auto& pair : _peers) {
			if (pair.second.needs_update()) {
				Profiler* prof = this_server.factory()->new_kernel<Profiler>();
				prof->to(pair.first);
				prof->set_principal_id(pair.first.address());
				profs.push_back(prof);
			}
		}
		if (profs.empty()) {
			commit(this_server.local_server());
		} else {
			_num_sent += profs.size();
			for (Profiler* prof : profs) {
				upstream(this_server.remote_server(), prof);
			}
		}
	}

	void react(Server& this_server, Kernel* k) override {
		Profiler* prof = dynamic_cast<Profiler*>(k);
		Peer& p = _peers[prof->from()];
		if (k->result() != Result::SUCCESS) {
			p.collect_error(1);
		} else {
			p.collect_sample(prof->time());
		}
		if (p.needs_update()) {
			Profiler* prof2 = this_server.factory()->new_kernel<Profiler>();
			prof2->to(prof->from());
			++_num_sent;
			upstream(this_server.remote_server(), prof2);
		}
		prof->copy_peers_to(_peers);
		if (--_num_sent == 0) {
			commit(this_server.local_server());
		}
	}

	const Peers& peers() const { return _peers; }

private:
	Peers _peers;
	size_t _num_sent = 0;
};

struct Negotiator: public Kernel, public Identifiable_tag {

	typedef stdx::log<Negotiator> this_log;

	Negotiator():
		_old_principal(), _new_principal() {}

	Negotiator(sysx::endpoint old, sysx::endpoint neww):
		_old_principal(old), _new_principal(neww) {}

	void negotiate(Server& this_server, Peers& peers) {
		stdx::log_func<this_log>(__func__, "new_principal", _new_principal);
		this->principal(this->parent());
		this->result(Result::SUCCESS);
		sysx::endpoint this_addr = peers.this_addr();
		if (_new_principal == this_addr) {
			this_log() << "Hello" << std::endl;
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
		this_server.remote_server()->send(this);
	}

	void write(sysx::packetstream& out) override {
		Kernel::write(out);
		// TODO: if moves_upstream
		out << _old_principal << _new_principal << _stop;
	}

	void read(sysx::packetstream& in) override {
		Kernel::read(in);
		in >> _old_principal >> _new_principal >> _stop;
	}

	bool stop() const { return _stop; }

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			8,
			"Negotiator",
			[] (sysx::packetstream& in) {
				Negotiator* k = new Negotiator;
				k->read(in);
				return k;
			}
		};
	}

private:
	sysx::endpoint _old_principal;
	sysx::endpoint _new_principal;
	bool _stop = false;
};

struct Master_negotiator: public Kernel, public Identifiable_tag {

	typedef stdx::log<Master_negotiator> this_log;

	Master_negotiator(sysx::endpoint old, sysx::endpoint neww):
		_old_principal(old), _new_principal(neww) {}

	void act(Server& this_server) override {
		if (_old_principal) {
			send_negotiator(this_server, _old_principal);
		}
		send_negotiator(this_server, _new_principal);
	}

	void react(Server& this_server, Kernel* k) override {
		this_log()
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
			this_server.local_server()->send(this);
		}
	}

	sysx::endpoint old_principal() const { return _old_principal; }

private:

	void send_negotiator(Server& this_server, sysx::endpoint addr) {
		++_num_sent;
		Negotiator* n = this_server.factory()->new_kernel<Negotiator>(_old_principal, _new_principal);
		n->set_principal_id(addr.address());
		n->to(addr);
		upstream(this_server.remote_server(), n);
	}

	sysx::endpoint _old_principal;
	sysx::endpoint _new_principal;
	uint8_t _num_sent = 0;
};

struct Shutdown_after: public Kernel {

	typedef stdx::log<Shutdown_after> this_log;

	explicit
	Shutdown_after(const Peers& peers) noexcept:
	_peers(peers)
	{}

	void act(Server& this_server) override {
		auto* __factory = this_server.factory();
		this_log() << "Hail the new king! addr="
			<< _peers.this_addr()
			<< ",peers=" << this->_peers
			<< ",npeers=" << all_peers.size() << std::endl;
		__factory->shutdown();
	}

private:

	const Peers& _peers;
};

struct Master_discoverer: public Kernel, public Identifiable_tag {

	typedef stdx::log<Master_discoverer> this_log;

	Master_discoverer(const Master_discoverer&) = delete;
	Master_discoverer& operator=(const Master_discoverer&) = delete;

	explicit
	Master_discoverer(sysx::endpoint this_addr):
	_peers(this_addr),
//	_cache(_peers.this_addr(), _peers),
	_scanner(nullptr),
	_discoverer(nullptr),
	_negotiator(nullptr)
	{
	}

	void act(Server& this_server) override {
		prog_start = current_time_nano();

		Shutdown_after* shutdowner = new Shutdown_after(_peers);
		shutdowner->after(std::chrono::seconds(10));
		shutdowner->parent(this);
		this_server.timer_server()->send(shutdowner);

		this_log()
			<< "startTime.push("
			<< current_time_nano() - prog_start
			<< "*1e-6); // "
			<< _peers.this_addr()
			<< std::endl;
		if (!all_peers.empty() && _peers.this_addr() != all_peers[0]) {
			run_scan(this_server);
		}
	}

	void react(Server& this_server, Kernel* k) override {
		if (_scanner == k) {
			if (k->result() != Result::SUCCESS) {
//				Time wait_time = sysx::this_process::getenv("WAIT_TIME", Time(60000000000UL));
//				if (current_time_nano() - prog_start > wait_time) {
//					this_log() << "Hail the new king "
//						<< _peers.this_addr() << "! npeers = " << all_peers.size() << std::endl;
//					__factory.stop();
//				}
				run_scan(this_server, _scanner->discovered_node());
			} else {
				change_principal(this_server, _scanner->discovered_node());
				run_discovery(this_server);
				_scanner = nullptr;
			}
		} else
		if (_discoverer == k) {
			if (k->result() == Result::SUCCESS) {
				Discoverer* dsc = dynamic_cast<Discoverer*>(k);
				_peers.update_peers(dsc->peers());
				_peers.debug();
				change_principal(this_server, _peers.best_peer());
			} else {
				run_discovery(this_server);
			}
		} else
		if (_negotiator == k) {
			if (k->result() != Result::SUCCESS) {
				Master_negotiator* neg = dynamic_cast<Master_negotiator*>(k);
				sysx::endpoint princ = _peers.principal();
				_peers.revert_principal(neg->old_principal());
				run_negotiator(this_server, princ, _peers.principal());
			}
			_peers.debug();
			_negotiator = nullptr;
		} else
		if (k->type()) {
			if (k->type() == Profiler::static_type()) {
				Profiler* prof = dynamic_cast<Profiler*>(k);
				prof->copy_peers_from(_peers);
				prof->act(this_server);
			} else if (k->type() == Negotiator::static_type()) {
				Negotiator* neg = dynamic_cast<Negotiator*>(k);
				neg->negotiate(this_server, _peers);
			}
		}
	}

private:

	void run_scan(Server& this_server, sysx::endpoint old_addr = sysx::endpoint()) {
		_scanner = this_server.factory()->new_kernel<Scanner>(_peers.this_addr(), old_addr);
		if (!old_addr) {
			_peers.add_peer(_scanner->scan_addr());
		}
		upstream(this_server.local_server(), _scanner);
	}

	void run_discovery(Server& this_server) {
		this_log() << "Discovering..." << std::endl;
//		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		upstream(this_server.local_server(), _discoverer = this_server.factory()->new_kernel<Discoverer>(_peers));
	}

	void run_negotiator(Server& this_server, sysx::endpoint old_princ, sysx::endpoint new_princ) {
		upstream(this_server.local_server(), _negotiator = this_server.factory()->new_kernel<Master_negotiator>(old_princ, new_princ));
	}

	void change_principal(Server& this_server, sysx::endpoint new_princ) {
		sysx::endpoint old_princ = _peers.principal();
		if (!_negotiator && _peers.change_principal(new_princ)) {
			this_log() << "Changing principal to " << new_princ << std::endl;
			_peers.debug();
			run_negotiator(this_server, old_princ, new_princ);
		}
	}

	Peers _peers;
//	discovery::Cache_guard<Peers> _cache;

	Scanner* _scanner;
	Discoverer* _discoverer;
	Master_negotiator* _negotiator;

	static const uint32_t MIN_SAMPLES = 7;
};


uint32_t num_peers() {
	uint32_t n = sysx::this_process::getenv("NUM_PEERS", 3);
	if (n <= 1) {
		n = 3;
	}
	return n;
}

struct test_discovery {};
struct generate_peers {};

void generate_all_peers(uint32_t npeers, sysx::ipv4_addr base_ip) {
	typedef stdx::log<generate_peers> this_log;
	all_peers.clear();
	const uint32_t start = sysx::to_host_format<uint32_t>(base_ip.rep());
	const uint32_t end = start + npeers;
	for (uint32_t i=start; i<end; ++i) {
		sysx::endpoint endpoint(sysx::ipv4_addr(sysx::to_network_format<uint32_t>(i)), DISCOVERY_PORT);
		all_peers.push_back(endpoint);
	}
	for (sysx::endpoint addr : all_peers) {
		auto it = std::min_element(
			all_peers.begin(), all_peers.end(),
			Compare_distance(addr)
		);
		this_log() << "Best link: " << addr << " -> " << *it << std::endl;
	}
//	uint32_t p = 1;
//	uint32_t fanout = 1 << p;
//	for (sysx::endpoint addr : all_peers) {
//		uint32_t pos = addr.position(my_netmask());
//		uint32_t lvl = log(pos, p);
//		uint32_t rem = pos - (1 << lvl);
//		auto dist = addr_distance(all_peers[7], addr);
//		this_log()
//			<< "Netmask = "
//			<< addr << ", "
//			<< sysx::endpoint(my_netmask(), 0) << ", "
//			<< pos << ", "
//			<< lvl << ":" << rem << " --> "
//			<< lvl-1 << ":" << rem/fanout << ", distance = "
//			<< dist.first << ',' << dist.second
//			<< std::endl;
//	}
}

struct Main: public Kernel {
	typedef stdx::log<test_discovery> this_log;
	Main(Server& this_server, int argc, char* argv[]):
	npeers(3),
	bind_addr(components::get_bind_address(), DISCOVERY_PORT),
	base_ip{127,0,0,1}
	{
		sysx::cmdline cmd(argc, argv, {
			sysx::cmd::ignore_first_arg(),
			sysx::cmd::make_option({"--bind-addr"}, bind_addr),
			sysx::cmd::make_option({"--num-peers"}, npeers),
			sysx::cmd::make_option({"--base-ip"}, base_ip)
		});
		try {
			cmd.parse();
		} catch (sysx::invalid_cmdline_argument& err) {
			std::cerr << err.what() << ": " << err.arg() << std::endl;
			std::exit(1);
		}
//		cmdline.parse([this](const std::string& arg, std::istream& in) {
//			     if (arg == "--bind-addr") { in >> bind_addr; }
//			else if (arg == "--num-peers") { in >> npeers; }
//			else if (arg == "--base-ip")   { in >> base_ip; }
//		});
		this_log() << "Bind address = " << bind_addr << std::endl;
		this_log() << "Num peers = " << npeers << std::endl;
		this_log() << "Base ip = " << base_ip << std::endl;
//		std::exit(0);
		generate_all_peers(npeers, base_ip);
		if (sysx::endpoint(base_ip,0).address() == bind_addr.address()) {
			graph.add_nodes(all_peers.begin(), all_peers.end());
		}
		auto& __factory = *this_server.factory();
		this_server.local_server()->add_cpu(0);
		this_server.remote_server()->socket(bind_addr);
		__factory.types().register_type(Profiler::static_type());
		__factory.types().register_type(Ping::static_type());
		__factory.types().register_type(Negotiator::static_type());
	}

	void act(Server& this_server) {
//		std::clog << "Hello from child process" << std::endl;
//		commit(this_server.local_server());
//		std::exit(0);
		const Time default_delay = (bind_addr == sysx::endpoint("127.0.0.1", DISCOVERY_PORT)) ? 0 : 2;
		const Time start_delay = sysx::this_process::getenv("START_DELAY", default_delay);
		Master_discoverer* master = new Master_discoverer(bind_addr);
		master->id(bind_addr.address());
		this_server.factory()->instances().register_instance(master);
		master->after(std::chrono::seconds(start_delay));
//		master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
		this_server.timer_server()->send(master);
	}

private:
	uint32_t npeers;
	sysx::endpoint bind_addr;
	sysx::ipv4_addr base_ip;
};

int main(int argc, char* argv[]) {
	int retval = -1;
	if (argc <= 2) {
//		factory::components::Interval_set<sysx::addr4_type> networks = factory::components::enumerate_networks();
//		networks.for_each([] (sysx::addr4_type rhs) { std::cout << sysx::ipv4_addr(sysx::to_network_format(rhs)) << std::endl; });
//		exit(0);
		typedef stdx::log<test_discovery> this_log;
		uint32_t npeers = num_peers();
//		std::string base_ip_str = argc == 2 ? argv[1] : "127.0.0.1";
//		sysx::ipv4_addr base_ip;
//		std::stringstream str;
//		str << base_ip_str;
//		str >> base_ip;
		sysx::ipv4_addr base_ip{127,0,0,1};
		generate_all_peers(npeers, base_ip);

		sysx::process_group procs;
		int start_id = 1000;
		for (sysx::endpoint endpoint : all_peers) {
			procs.add([endpoint, &argv, start_id, npeers, &base_ip] () {
				sysx::this_process::env("START_ID", start_id);
				return sysx::this_process::execute(argv[0],
					"--bind-addr", endpoint,
					"--num-peers", npeers,
					"--base-ip", base_ip);
			});
			start_id += 1000;
		}

		this_log() << "Forked " << procs << std::endl;

		retval = procs.wait();
	} else {
		using namespace factory;
		retval = factory_main<Main,config>(argc, argv);
	}
	return retval;
}
