#include <fstream>
#include <map>

#include <sysx/socket.hh>
#include <sysx/cmdline.hh>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

#include "network.hh"
#include "distance.hh"
#include "cache_guard.hh"
#include "hierarchy.hh"
#include "springy_graph_generator.hh"
#include "hierarchy_with_graph.hh"
#include "test.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::true_type {};

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

template<class Address>
struct Delayed_shutdown: public Kernel {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	typedef stdx::log<Delayed_shutdown> this_log;

	explicit
	Delayed_shutdown(const hierarchy_type& peers) noexcept:
	_hierarchy(peers)
	{}

	void
	act(Server& this_server) override {
		this_log() << "Hail the king! His hoes: " << _hierarchy << std::endl;
		this_server.factory()->shutdown();
	}

private:

	const hierarchy_type& _hierarchy;

};

template<class Address>
struct Negotiator: public Kernel, public Identifiable_tag {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	typedef stdx::log<Negotiator> this_log;

	Negotiator() noexcept:
	_oldprinc(),
	_newprinc()
	{}

	Negotiator(sysx::endpoint old, sysx::endpoint neww) noexcept:
	_oldprinc(old),
	_newprinc(neww)
	{}

	template<class Hierarchy>
	void
	negotiate(Server& this_server, Hierarchy& hierarchy) {
		const sysx::endpoint& this_addr = hierarchy.bindaddr();
		this->principal(this->parent());
		this->result(Result::SUCCESS);
		if (_newprinc == this_addr) {
			// principal becomes subordinate
			if (this->from() == hierarchy.principal()) {
				if (_oldprinc) {
					hierarchy.unset_principal();
				} else {
					// root tries to swap with its subordinate
					this->result(Result::USER_ERROR);
				}
			}
			if (this->result() != Result::USER_ERROR) {
				hierarchy.add_subordinate(this->from());
			}
		} else
		if (_oldprinc == this_addr) {
			// something fancy is going on
			if (this->from() == hierarchy.principal()) {
				this->result(Result::USER_ERROR);
			} else {
				hierarchy.remove_subordinate(this->from());
			}
		}
		this_server.remote_server()->send(this);
	}

	void write(sysx::packetstream& out) override {
		Kernel::write(out);
		// TODO: if moves_upstream
		out << _oldprinc << _newprinc;
	}

	void read(sysx::packetstream& in) override {
		Kernel::read(in);
		in >> _oldprinc >> _newprinc;
	}

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
				Negotiator<Address>* k = new Negotiator<Address>;
				k->read(in);
				return k;
			}
		};
	}

private:

	sysx::endpoint _oldprinc;
	sysx::endpoint _newprinc;

};

template<class Address>
struct Master_negotiator: public Kernel, public Identifiable_tag {

	typedef Address addr_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef stdx::log<Master_negotiator> this_log;

	Master_negotiator(sysx::endpoint oldp, sysx::endpoint newp):
	_oldprinc(oldp),
	_newprinc(newp)
	{}

	void
	act(Server& this_server) override {
		send_negotiator(this_server, _newprinc);
	}

	void
	react(Server& this_server, Kernel* k) override {
		bool finished = true;
		if (_numsent == 1) {
			this_log() << "Tried " << k->from() << ": " << k->result() << std::endl;
			this->result(k->result());
			if (k->result() == Result::SUCCESS and _oldprinc) {
				finished = false;
				send_negotiator(this_server, _oldprinc);
			}
		}
		if (finished) {
			this->principal(this->parent());
			this_server.local_server()->send(this);
		}
	}

	const sysx::endpoint&
	old_principal() const noexcept {
		return _oldprinc;
	}

	const sysx::endpoint&
	new_principal() const noexcept {
		return _newprinc;
	}

private:

	void
	send_negotiator(Server& this_server, sysx::endpoint addr) {
		++_numsent;
		negotiator_type* n = this_server.factory()->new_kernel<negotiator_type>(_oldprinc, _newprinc);
		n->set_principal_id(addr.address());
		n->to(addr);
		upstream(this_server.remote_server(), n);
	}

	sysx::endpoint _oldprinc;
	sysx::endpoint _newprinc;
	uint32_t _numsent = 0;
};


template<class Address>
struct Master_discoverer: public Kernel, public Identifiable_tag {

	typedef Address addr_type;
	typedef discovery::Network<addr_type> network_type;
	typedef discovery::Hierarchy_with_graph<discovery::Hierarchy<addr_type>> hierarchy_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef std::multimap<distance_type,addr_type> rankedlist_type;
	typedef typename rankedlist_type::iterator rankedlist_iterator;
	typedef Negotiator<addr_type> negotiator_type;
	typedef stdx::log<Master_discoverer> this_log;

	Master_discoverer(const network_type& network, const sysx::port_type port):
	_hierarchy(network, port),
	_port(port),
	_rankedhosts(),
	_currenthost(),
	_negotiator(nullptr),
	_graph()
//	_cache(_hierarchy.this_addr(), _hierarchy),
	{
		_hierarchy.set_graph(_graph);
		generate_ranked_hosts(network);
	}

	Master_discoverer(const Master_discoverer&) = delete;
	Master_discoverer& operator=(const Master_discoverer&) = delete;

	void
	act(Server& this_server) override {
		schedule_shutdown_after(std::chrono::seconds(10), this_server);
		try_next_host(this_server);
	}

	void
	react(Server& this_server, Kernel* k) override {
		if (_negotiator == k) {
			if (k->result() == Result::SUCCESS) {
				_hierarchy.set_principal(_negotiator->new_principal());
				_negotiator = nullptr;
			} else {
				try_next_host(this_server);
			}
		} else
		if (k->type() == negotiator_type::static_type()) {
			negotiator_type* neg = dynamic_cast<negotiator_type*>(k);
			neg->negotiate(this_server, _hierarchy);
		}
	}

private:

	void
	generate_ranked_hosts(const network_type& rhs) {
		const auto fanout = 2;
		_rankedhosts.clear();
		std::transform(
			rhs.begin(), rhs.middle(),
			std::inserter(_rankedhosts, _rankedhosts.end()),
			[&rhs,fanout] (const addr_type& to) {
				const distance_type dist = distance_type(rhs.address(), to, rhs.netmask(), fanout);
				return std::make_pair(dist, to);
			}
		);
		_currenthost = _rankedhosts.end();
	}

	void
	try_next_host(Server& this_server) {
		advance_through_ranked_list();
		run_negotiator(this_server);
	}

	void
	advance_through_ranked_list() {
		wrap_around();
		skip_this_host();
	}

	void
	wrap_around() {
		if (_currenthost == _rankedhosts.end()) {
			_currenthost = _rankedhosts.begin();
		} else {
			++_currenthost;
		}
	}

	void
	skip_this_host() {
		if (_currenthost != _rankedhosts.end() and _currenthost->second == _hierarchy.network().address()) {
			++_currenthost;
		}
	}

	template<class Time>
	void
	schedule_shutdown_after(Time delay, Server& this_server) {
		Delayed_shutdown<addr_type>* shutdowner = new Delayed_shutdown<addr_type>(_hierarchy);
		shutdowner->after(delay);
		shutdowner->parent(this);
		this_server.timer_server()->send(shutdowner);
	}

	void
	run_negotiator(Server& this_server) {
		if (_currenthost != _rankedhosts.end()) {
			const sysx::endpoint new_princ(_currenthost->second, _port);
			this_log() << "trying " << new_princ << std::endl;
			_negotiator = this_server.factory()->new_kernel<Master_negotiator<addr_type>>(_hierarchy.principal(), new_princ);
			upstream(this_server.local_server(), _negotiator);
		}
	}

	hierarchy_type _hierarchy;
	sysx::port_type _port;
	rankedlist_type _rankedhosts;
	rankedlist_iterator _currenthost;
//	discovery::Cache_guard<Peers> _cache;

	Master_negotiator<addr_type>* _negotiator;
	springy::Springy_graph _graph;
};

struct test_discovery {};

template<class Address>
struct Main: public Kernel {

	typedef Address addr_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef stdx::log<test_discovery> this_log;

	Main(Server& this_server, int argc, char* argv[]):
	_network(),
	_port(),
	_cmdline(argc, argv, {
		sysx::cmd::ignore_first_arg(),
		sysx::cmd::ignore_arg("--num-peers"),
		sysx::cmd::ignore_arg("--role"),
		sysx::cmd::make_option({"--network"}, _network),
		sysx::cmd::make_option({"--port"}, _port)
	})
	{}

	void
	act(Server& this_server) {
		parse_cmdline_args(this_server);
		this_server.factory()->types().register_type(negotiator_type::static_type());
		if (this_server.factory()->exit_code()) {
			commit(this_server.local_server());
		} else {
			const sysx::endpoint bind_addr(_network.address(), _port);
			this_server.remote_server()->socket(bind_addr);
			std::clog << "Hello from child process" << std::endl;
			const auto default_delay = (_network.address() == sysx::ipv4_addr{127,0,0,1}) ? 0 : 2;
			const auto start_delay = sysx::this_process::getenv("START_DELAY", default_delay);
			Master_discoverer<sysx::ipv4_addr>* master = new Master_discoverer<sysx::ipv4_addr>(_network, _port);
			master->id(sysx::to_host_format(_network.address().rep()));
			this_server.factory()->instances().register_instance(master);
			master->after(std::chrono::seconds(start_delay));
//			master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
			this_server.timer_server()->send(master);
		}
	}

private:

	void
	parse_cmdline_args(Server& this_server) {
		try {
			_cmdline.parse();
			if (!_network) {
				throw sysx::invalid_cmdline_argument("--network");
			}
		} catch (sysx::invalid_cmdline_argument& err) {
			std::cerr << err.what() << ": " << err.arg() << std::endl;
			this_server.factory()->set_exit_code(1);
		}
	}

	discovery::Network<sysx::ipv4_addr> _network;
	sysx::port_type _port;
	sysx::cmdline _cmdline;

};

int main(int argc, char* argv[]) {

	const std::string role_master = "master";
	const std::string role_slave = "slave";
	sysx::port_type discovery_port = 10000;

	typedef stdx::log<test_discovery> this_log;
	int retval = 0;
	discovery::Network<sysx::ipv4_addr> network;
	uint32_t npeers = 0;
	std::string role;
	try {
		sysx::cmdline cmd(argc, argv, {
			sysx::cmd::ignore_first_arg(),
			sysx::cmd::make_option({"--network"}, network),
			sysx::cmd::make_option({"--num-peers"}, npeers),
			sysx::cmd::make_option({"--role"}, role),
			sysx::cmd::make_option({"--port"}, discovery_port)
		});
		cmd.parse();
		if (role != role_master and role != role_slave) {
			throw sysx::invalid_cmdline_argument("--role");
		}
		if (!network) {
			throw sysx::invalid_cmdline_argument("--network");
		}
	} catch (sysx::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		return 1;
	}

	if (role == role_master) {

		this_log() << "Network = " << network << std::endl;
		this_log() << "Num peers = " << npeers << std::endl;
		this_log() << "Role = " << role << std::endl;
		this_log() << "start,end = " << *network.begin() << ',' << *network.end() << std::endl;

		std::vector<sysx::endpoint> hosts;
		std::transform(
			network.begin(),
			network.begin() + npeers,
			std::back_inserter(hosts),
			[discovery_port] (const sysx::ipv4_addr& addr) {
				return sysx::endpoint(addr, discovery_port);
			}
		);
		springy::Springy_graph graph;
		graph.add_nodes(hosts.begin(), hosts.end());

		sysx::process_group procs;
		int start_id = 1000;
		for (sysx::endpoint endpoint : hosts) {
			procs.add([endpoint, &argv, start_id, npeers, &network, discovery_port] () {
				sysx::this_process::env("START_ID", start_id);
				return sysx::this_process::execute(
					argv[0],
					"--network", discovery::Network<sysx::ipv4_addr>(endpoint.addr4(), network.netmask()),
					"--port", discovery_port,
					"--role", "slave",
					"--num-peers", 0
				);
			});
			start_id += 1000;
		}

		this_log() << "Forked " << procs << std::endl;

		retval = procs.wait();
	} else {
		using namespace factory;
		retval = factory_main<Main<sysx::ipv4_addr>,config>(argc, argv);
	}

	return retval;
}
