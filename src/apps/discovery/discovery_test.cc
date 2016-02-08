#include <fstream>

#include <sysx/socket.hh>
#include <sysx/cmdline.hh>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

#include "discovery.hh"
#include "distance.hh"
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


//struct Negotiator: public Kernel, public Identifiable_tag {
//
//	typedef stdx::log<Negotiator> this_log;
//
//	Negotiator() noexcept:
//	_old_principal(),
//	_new_principal()
//	{}
//
//	Negotiator(sysx::endpoint old, sysx::endpoint neww) noexcept:
//	_old_principal(old),
//	_new_principal(neww)
//	{}
//
//	void negotiate(Server& this_server, Peers& peers) {
//		stdx::log_func<this_log>(__func__, "new_principal", _new_principal);
//		this->principal(this->parent());
//		this->result(Result::SUCCESS);
//		sysx::endpoint this_addr = peers.this_addr();
//		if (_new_principal == this_addr) {
//			this_log() << "Hello" << std::endl;
//			// principal becomes subordinate
//			if (this->from() == peers.principal()) {
//				if (_old_principal) {
//					peers.remove_principal();
//				} else {
//					// root tries to swap with its subordinate
//					this->result(Result::USER_ERROR);
//				}
//			}
//			if (this->result() != Result::USER_ERROR) {
//				peers.add_subordinate(this->from());
//			}
//		} else
//		if (_old_principal == this_addr) {
//			// something fancy is going on
//			if (this->from() == peers.principal()) {
//				this->result(Result::USER_ERROR);
//			} else {
//				peers.remove_subordinate(this->from());
//			}
//		}
//		_stop = !peers.principal() && peers.num_subordinates() == all_peers.size()-1;
//		this_server.remote_server()->send(this);
//	}
//
//	void write(sysx::packetstream& out) override {
//		Kernel::write(out);
//		// TODO: if moves_upstream
//		out << _old_principal << _new_principal << _stop;
//	}
//
//	void read(sysx::packetstream& in) override {
//		Kernel::read(in);
//		in >> _old_principal >> _new_principal >> _stop;
//	}
//
//	bool stop() const { return _stop; }
//
//	const Type<Kernel>
//	type() const noexcept override {
//		return static_type();
//	}
//
//	static const Type<Kernel>
//	static_type() noexcept {
//		return Type<Kernel>{
//			8,
//			"Negotiator",
//			[] (sysx::packetstream& in) {
//				Negotiator* k = new Negotiator;
//				k->read(in);
//				return k;
//			}
//		};
//	}
//
//private:
//	sysx::endpoint _old_principal;
//	sysx::endpoint _new_principal;
//	bool _stop = false;
//};
//
//struct Master_negotiator: public Kernel, public Identifiable_tag {
//
//	typedef stdx::log<Master_negotiator> this_log;
//
//	Master_negotiator(sysx::endpoint old, sysx::endpoint neww):
//		_old_principal(old), _new_principal(neww) {}
//
//	void act(Server& this_server) override {
//		if (_old_principal) {
//			send_negotiator(this_server, _old_principal);
//		}
//		send_negotiator(this_server, _new_principal);
//	}
//
//	void react(Server& this_server, Kernel* k) override {
//		this_log()
//			<< "Negotiator returned from " << k->from()
//			<< " with result=" << k->result() << std::endl;
////		if (k->from() == _new_principal && k->result() != Result::SUCCESS) {
//		if (this->result() == Result::UNDEFINED && k->result() != Result::SUCCESS) {
//			this->result(k->result());
//		}
////		Negotiator* neg = dynamic_cast<Negotiator*>(k);
//		if (--_num_sent == 0) {
//			if (this->result() == Result::UNDEFINED) {
//				this->result(Result::SUCCESS);
//			}
//			this->principal(this->parent());
//			this_server.local_server()->send(this);
//		}
//	}
//
//	sysx::endpoint old_principal() const { return _old_principal; }
//
//private:
//
//	void send_negotiator(Server& this_server, sysx::endpoint addr) {
//		++_num_sent;
//		Negotiator* n = this_server.factory()->new_kernel<Negotiator>(_old_principal, _new_principal);
//		n->set_principal_id(addr.address());
//		n->to(addr);
//		upstream(this_server.remote_server(), n);
//	}
//
//	sysx::endpoint _old_principal;
//	sysx::endpoint _new_principal;
//	uint8_t _num_sent = 0;
//};
//
//struct Shutdown_after: public Kernel {
//
//	typedef stdx::log<Shutdown_after> this_log;
//
//	explicit
//	Shutdown_after(const Peers& peers) noexcept:
//	_peers(peers)
//	{}
//
//	void act(Server& this_server) override {
//		auto* __factory = this_server.factory();
//		this_log() << "Hail the new king! addr="
//			<< _peers.this_addr()
//			<< ",peers=" << this->_peers
//			<< ",npeers=" << all_peers.size() << std::endl;
//		__factory->shutdown();
//	}
//
//private:
//
//	const Peers& _peers;
//};
//
//struct Master_discoverer: public Kernel, public Identifiable_tag {
//
//	typedef stdx::log<Master_discoverer> this_log;
//
//	Master_discoverer(const Master_discoverer&) = delete;
//	Master_discoverer& operator=(const Master_discoverer&) = delete;
//
//	explicit
//	Master_discoverer(sysx::endpoint this_addr):
//	_peers(this_addr),
////	_cache(_peers.this_addr(), _peers),
//	_negotiator(nullptr)
//	{}
//
//	void act(Server& this_server) override {
//
//		Shutdown_after* shutdowner = new Shutdown_after(_peers);
//		shutdowner->after(std::chrono::seconds(10));
//		shutdowner->parent(this);
//		this_server.timer_server()->send(shutdowner);
//
//		if (!all_peers.empty() && _peers.this_addr() != all_peers[0]) {
//			run_scan(this_server);
//		}
//	}
//
//	void react(Server& this_server, Kernel* k) override {
//		if (_negotiator == k) {
//			if (k->result() != Result::SUCCESS) {
//				Master_negotiator* neg = dynamic_cast<Master_negotiator*>(k);
//				sysx::endpoint princ = _peers.principal();
//				_peers.revert_principal(neg->old_principal());
//				run_negotiator(this_server, princ, _peers.principal());
//			}
//			_peers.debug();
//			_negotiator = nullptr;
//		} else
//		if (k->type() == Negotiator::static_type()) {
//			Negotiator* neg = dynamic_cast<Negotiator*>(k);
//			neg->negotiate(this_server, _peers);
//		}
//	}
//
//private:
//
//	void run_negotiator(Server& this_server, sysx::endpoint old_princ, sysx::endpoint new_princ) {
//		upstream(this_server.local_server(), _negotiator = this_server.factory()->new_kernel<Master_negotiator>(old_princ, new_princ));
//	}
//
//	void change_principal(Server& this_server, sysx::endpoint new_princ) {
//		sysx::endpoint old_princ = _peers.principal();
//		if (!_negotiator && _peers.change_principal(new_princ)) {
//			this_log() << "Changing principal to " << new_princ << std::endl;
//			_peers.debug();
//			run_negotiator(this_server, old_princ, new_princ);
//		}
//	}
//
//	discovery::Hierarchy<sysx::ipv4_addr> _peers;
////	discovery::Cache_guard<Peers> _cache;
//
//	Master_negotiator* _negotiator;
//	springy::Springy_graph _graph;
//};
//
//
//
struct test_discovery {};
//struct generate_peers {};
//
template<class Result>
void
enumerate_hosts(discovery::Network<sysx::ipv4_addr> network, uint32_t npeers, sysx::port_type discovery_port, Result result) {
	typedef sysx::ipv4_addr::rep_type rep_type;
	const rep_type start = network.start();
	const rep_type end = start + npeers;
	for (rep_type i=start; i<end; ++i) {
		*result = sysx::endpoint(sysx::ipv4_addr(sysx::to_network_format(i)), discovery_port);
		++result;
	}
}

struct Main: public Kernel {
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
	{
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

	void act(Server& this_server) {
//		this_server.factory()->types().register_type(Negotiator::static_type());
		if (this_server.factory()->exit_code()) {
			commit(this_server.local_server());
		} else {
			const sysx::endpoint bind_addr(_network.address(), _port);
			this_server.remote_server()->socket(bind_addr);
			std::clog << "Hello from child process" << std::endl;
			commit(this_server.local_server());
		}
//		const Time default_delay = (bind_addr == sysx::endpoint("127.0.0.1", DISCOVERY_PORT)) ? 0 : 2;
//		const Time start_delay = sysx::this_process::getenv("START_DELAY", default_delay);
//		Master_discoverer* master = new Master_discoverer(bind_addr);
//		master->id(bind_addr.address());
//		this_server.factory()->instances().register_instance(master);
//		master->after(std::chrono::seconds(start_delay));
////		master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
//		this_server.timer_server()->send(master);
	}

private:

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
		this_log() << "start,end = " << network.start() << ',' << network.end() << std::endl;

		std::vector<sysx::endpoint> hosts;
		enumerate_hosts(
			network, npeers,
			discovery_port,
			std::back_inserter(hosts)
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
		retval = factory_main<Main,config>(argc, argv);
	}

	return retval;
}
