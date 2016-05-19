#include <stdx/debug.hh>

#include <map>

#include <sys/socket.hh>
#include <sys/cmdline.hh>
#include <sys/ifaddr.hh>

#include <factory/factory.hh>
#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/io_server.hh>
#include <factory/nic_server.hh>

#include "distance.hh"
#include "cache_guard.hh"
#include "hierarchy.hh"
#include <springy/springy.hh>
#include "hierarchy_with_graph.hh"
#include "test.hh"

namespace factory {

	struct Router {

		void
		send_local(Kernel* rhs) {
			local_server.send(rhs);
		}

		void
		send_remote(Kernel*);

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
			assert(false);
		}

	};

	NIC_server<Kernel, sys::socket, Router> remote_server;

	void
	Router::send_remote(Kernel* rhs) {
		remote_server.send(rhs);
	}

}

using namespace factory;

#include "apps/autoreg/autoreg_app.hh"
#include "apps/spec/spec_app.hh"
#include "ping_pong.hh"
#include "delayed_shutdown.hh"
#include "master_discoverer.hh"

constexpr const char* role_master = "master";
constexpr const char* role_slave = "slave";
constexpr const uint32_t do_not_kill = std::numeric_limits<uint32_t>::max();

template<class Address>
struct Main: public Kernel {

	typedef Address addr_type;
	typedef typename sys::ipaddr_traits<addr_type> traits_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef discovery::Hierarchy_with_graph<discovery::Hierarchy<addr_type>> hierarchy_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef Master_discoverer<addr_type, hierarchy_type, distance_type> discoverer_type;

	Main(int argc, char* argv[]):
	_cmdline(argc, argv, {
		sys::cmd::ignore_first_arg(),
		sys::cmd::make_option({"--role"}, _role),
		sys::cmd::make_option({"--network"}, _network),
		sys::cmd::make_option({"--port"}, _port),
		sys::cmd::make_option({"--timeout"}, _timeout),
		sys::cmd::make_option({"--normal"}, _normal),
		sys::cmd::make_option({"--master"}, _masteraddr)
	})
	{}

	void
	act() {
		parse_cmdline_args();
		factory::types.register_type<negotiator_type>();
		factory::types.register_type<Ping>();
		factory::types.register_type<autoreg::Generator1<float,autoreg::Uniform_grid>>();
		factory::types.register_type<autoreg::Wave_surface_generator<float,autoreg::Uniform_grid>>();
		factory::types.register_type<autoreg::Autoreg_model<float>>();
		factory::types.register_type<Secret_agent>();
		factory::types.register_type<Launcher>();
		factory::types.register_type<Year_kernel>();
		factory::types.register_type<Station_kernel>();
		factory::types.register_type<Spec_app>();
		if (this->result() != Result::success) {
			factory::commit(factory::local_server, this);
		} else {
			const sys::ipv4_addr netmask = sys::ipaddr_traits<sys::ipv4_addr>::loopback_mask();
			const sys::endpoint bind_addr(_network.address(), _port);
			factory::remote_server.bind(bind_addr, netmask);
			const auto start_delay = 5;
			discoverer_type* master = new discoverer_type(_network, _port);
			master->id(sys::to_host_format(_network.address().rep()));
			factory::instances.register_instance(master);
			master->after(std::chrono::seconds(start_delay));
			factory::timer_server.send(master);

//			if (_network.address() == traits_type::localhost()) {
//				schedule_pingpong_after(std::chrono::seconds(0));
//			}

			if (_network.address() == _masteraddr) {
//				schedule_autoreg_app();
				schedule_spec_app();
			}

			if (_timeout != do_not_kill) {
				schedule_shutdown_after(std::chrono::seconds(_timeout), master);
			}
		}
	}

private:

	template<class Time>
	void
	schedule_pingpong_after(Time delay) {
		Ping_pong* p = new Ping_pong(_numpings);
		p->after(delay);
		factory::timer_server.send(p);
	}

	template<class Time>
	void
	schedule_shutdown_after(Time delay, discoverer_type* master) {
		Delayed_shutdown<addr_type>* shutdowner = new Delayed_shutdown<addr_type>(master->hierarchy(), _normal);
		shutdowner->after(delay);
		shutdowner->parent(this);
		factory::timer_server.send(shutdowner);
	}

	void
	schedule_autoreg_app() {
		Autoreg_app* app = new Autoreg_app;
		app->after(std::chrono::seconds(10));
		factory::timer_server.send(app);
	}

	void
	schedule_spec_app() {
		Spec_app* app = new Spec_app;
		app->after(std::chrono::seconds(20));
		factory::timer_server.send(app);
	}

	void
	parse_cmdline_args() {
		try {
			_cmdline.parse();
			if (!_network) {
				throw sys::invalid_cmdline_argument("--network");
			}
			if (_role != role_slave) {
				throw sys::invalid_cmdline_argument("--role");
			}
		} catch (sys::invalid_cmdline_argument& err) {
			std::cerr << err.what() << ": " << err.arg() << std::endl;
			this->result(Result::error);
		}
	}

	std::string _role;
	sys::ifaddr<sys::ipv4_addr> _network;
	sys::port_type _port;
	uint32_t _numpings = 10;
	uint32_t _timeout = 60;
	bool _normal = true;
	sys::ipv4_addr _masteraddr;
	sys::cmdline _cmdline;

};

template<class Address>
struct Hosts: public std::vector<Address> {

	typedef Address addr_type;

	friend std::istream&
	operator>>(std::istream& cmdline, Hosts& rhs) {
		std::string filename;
		cmdline >> filename;
		std::clog << "reading hosts from " << filename << std::endl;
		std::ifstream in(filename);
		rhs.clear();
		std::copy(
			std::istream_iterator<addr_type>(in),
			std::istream_iterator<addr_type>(),
			std::back_inserter(rhs)
		);
		return cmdline;
	}

};

int main(int argc, char* argv[]) {

	sys::port_type discovery_port = 54321;
	uint32_t kill_after = do_not_kill;
	uint32_t kill_timeout = do_not_kill;
	Hosts<sys::ipv4_addr> hosts2;
	sys::ipv4_addr master_addr{127,0,0,1};
	sys::ipv4_addr kill_addr{127,0,0,2};

	int retval = 0;
	sys::ifaddr<sys::ipv4_addr> network;
	size_t nhosts = 0;
	std::string role = role_master;
	try {
		sys::cmdline cmd(argc, argv, {
			sys::cmd::ignore_first_arg(),
			sys::cmd::ignore_arg("--normal"),
			sys::cmd::make_option({"--hosts"}, hosts2),
			sys::cmd::make_option({"--network"}, network),
			sys::cmd::make_option({"--num-hosts"}, nhosts),
			sys::cmd::make_option({"--role"}, role),
			sys::cmd::make_option({"--port"}, discovery_port),
			sys::cmd::make_option({"--timeout"}, kill_timeout),
			sys::cmd::make_option({"--master"}, master_addr),
			sys::cmd::make_option({"--kill"}, kill_addr),
			sys::cmd::make_option({"--kill-after"}, kill_after)
		});
		cmd.parse();
		if (role != role_master and role != role_slave) {
			throw sys::invalid_cmdline_argument("--role");
		}
		if (!network) {
			throw sys::invalid_cmdline_argument("--network");
		}
	} catch (sys::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		return 1;
	}

	if (role == role_master) {

		#ifndef NDEBUG
		stdx::debug_message(
			"tst",
			"Network = _\n"
			"Num peers = _\n"
			"Role = _\n"
			"Start,mid = _,_",
			network, nhosts, role,
			*network.begin(), *network.middle()
		);
		#endif

		std::vector<sys::endpoint> hosts;
		if (network) {
			std::transform(
				network.begin(),
				network.begin() + nhosts,
				std::back_inserter(hosts),
				[discovery_port] (const sys::ipv4_addr& addr) {
					return sys::endpoint(addr, discovery_port);
				}
			);
		} else {
			std::transform(
				hosts2.begin(),
				hosts2.begin() + std::min(hosts2.size(), nhosts),
				std::back_inserter(hosts),
				[discovery_port] (const sys::ipv4_addr& addr) {
					return sys::endpoint(addr, discovery_port);
				}
			);
		}
		springy::Springy_graph<sys::endpoint> graph;
		graph.add_nodes(hosts.begin(), hosts.end());

		sys::process_group procs;
		for (sys::endpoint endpoint : hosts) {
			procs.emplace([endpoint, &argv, nhosts, &network, discovery_port, master_addr, kill_addr, kill_after, kill_timeout] () {
				char workdir[PATH_MAX];
				::getcwd(workdir, PATH_MAX);
				uint32_t timeout = kill_timeout;
				bool normal = true;
				if (endpoint.addr4() == kill_addr and kill_after != do_not_kill) {
					timeout = kill_after;
					normal = false;
				}
				return sys::this_process::execute(
					#if defined(FACTORY_TEST_USE_SSH)
					"/usr/bin/ssh", "-n", "-o", "StrictHostKeyChecking no", endpoint.addr4(),
					"cd", workdir, ';', "exec",
					#endif
					argv[0],
					"--network", sys::ifaddr<sys::ipv4_addr>(endpoint.addr4(), network.netmask()),
					"--port", discovery_port,
					"--role", "slave",
					"--timeout", timeout,
					"--normal", normal,
					"--master", master_addr
				);
			});
		}

		#ifndef NDEBUG
		stdx::debug_message("tst", "forked _", procs);
		#endif
		retval = procs.wait();

	} else {
		using namespace factory;
		local_server.send(new Main<sys::ipv4_addr>(argc, argv));
		retval = factory::wait_and_return();
	}

	return retval;
}
