#include <stdx/debug.hh>

#include <map>
#include <memory>

#include <sys/ioctl.h>

#include <sys/socket.hh>
#include <sys/cmdline.hh>
#include <sys/ifaddr.hh>
#include <sys/event.hh>
#include <sys/pipe.hh>
#include <sys/fildesbuf.hh>
#include <sys/argstream.hh>

#include <factory/factory.hh>
#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/io_server.hh>
#include <factory/nic_server.hh>
#include <factory/basic_router.hh>

#include "distance.hh"
#include "cache_guard.hh"
#include "hierarchy.hh"
#if defined(SPRINGY)
#include <springy/springy.hh>
#include "hierarchy_with_graph.hh"
#endif
#include "test.hh"

namespace factory {

	struct Router: public Basic_router<Kernel> {

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


#ifdef FACTORY_TEST
std::promise<void> __failure;
#include <future>
extern std::promise<void> __failure;
void
simulate_failure() {
	try {
		__failure.set_value();
	} catch (std::future_error& err) {
		// ignore errors
	}
}
#endif
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
	#if defined(SPRINGY)
	typedef discovery::Hierarchy_with_graph<discovery::Hierarchy<addr_type>> hierarchy_type;
	#else
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	#endif
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef Master_discoverer<addr_type, hierarchy_type, distance_type> discoverer_type;

	Main(int argc, char* argv[]):
	_cmdline(argc, argv, {
		sys::cmd::ignore_first_arg(),
		sys::cmd::make_option({"--role"}, _role),
		sys::cmd::make_option({"--network"}, _network),
		sys::cmd::make_option({"--port"}, _port),
		sys::cmd::make_option({"--test-duration"}, _timeout),
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
		if (this->result() == Result::error) {
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
				schedule_autoreg_app();
//				schedule_spec_app();
			}

			if (_timeout != do_not_kill) {
				_shutdown_wait_thread = std::thread([this,master] () {
					__failure.get_future().wait();
					schedule_shutdown_after(std::chrono::seconds(_timeout), master);
				});
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
	bool _normal = false;
	sys::ipv4_addr _masteraddr;
	sys::cmdline _cmdline;
	std::thread _shutdown_wait_thread;

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

struct Kill_addresses {

	friend std::istream&
	operator>>(std::istream& in, Kill_addresses& rhs) {
		std::stringstream token;
		char ch;
		in >> std::ws;
		bool finished = false;
		while (!finished && in) {
			ch = in.get();
			if (std::isspace(ch) || in.eof()) {
				in.putback(ch);
				finished = true;
			}
			if (ch == ',' || finished) {
				sys::ipv4_addr a;
				if (token >> a) {
					rhs._addrs.emplace_back(a);
					token.str("");
					token.clear();
				} else {
					in.setstate(std::ios::failbit);
				}
			} else {
				token.put(ch);
			}
		}
		return in;
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Kill_addresses& rhs) {
		std::copy(
			rhs._addrs.begin(),
			rhs._addrs.end(),
			stdx::intersperse_iterator<sys::ipv4_addr,char>(out, ',')
		);
		return out;
	}

	const std::vector<sys::ipv4_addr>&
	addrs() const noexcept {
		return _addrs;
	}

	bool
	contains(sys::ipv4_addr a) const noexcept {
		return std::find(_addrs.begin(), _addrs.end(), a) != _addrs.end();
	}

	bool
	contain(sys::ipv4_addr a) const noexcept {
		return contains(a);
	}

private:
	std::vector<sys::ipv4_addr> _addrs;
};


class Fail_over_test {

	sys::port_type discovery_port = 54321;
	uint32_t kill_after = do_not_kill;
	uint32_t _testduration = do_not_kill;
	Hosts<sys::ipv4_addr> hosts2;
	sys::ipv4_addr _master;
	Kill_addresses _victims;
	bool ssh = false;
	bool _netns = false;
	std::string _cluster;
	sys::ifaddr<sys::ipv4_addr> network;
	size_t nhosts = 0;
	std::string role = role_master;
	char** _argv;
	int retval = 0;

public:

	Fail_over_test(int argc, char* argv[]) {
		_argv = argv;
		parse_command_line(argc, argv);
	}

	int
	return_value() const {
		return retval;
	}

	void
	run(int argc, char* argv[]) {
		if (role == role_master) {
			run_master();
		} else {
			run_slave(argc, argv);
		}
	}

	void
	run_master() {

		#ifndef NDEBUG
		stdx::debug_message(
			"tst",
			"Network = _\n"
			"Num peers = _\n"
			"Role = _\n"
			"Start,mid = _,_\n"
			"Test duration = _\n"
			"Kill after = _\n"
			"Victims = _",
			network, nhosts, role,
			*network.begin(), *network.middle(),
			_testduration,
			kill_after,
			_victims
		);
		#endif

		std::vector<sys::endpoint> hosts;
		if (network) {
			std::transform(
				network.begin(),
				network.begin() + nhosts,
				std::back_inserter(hosts),
				[this] (const sys::ipv4_addr& addr) {
					return sys::endpoint(addr, discovery_port);
				}
			);
		} else {
			std::transform(
				hosts2.begin(),
				hosts2.begin() + std::min(hosts2.size(), nhosts),
				std::back_inserter(hosts),
				[this] (const sys::ipv4_addr& addr) {
					return sys::endpoint(addr, discovery_port);
				}
			);
		}
		#if defined(SPRINGY)
		springy::Springy_graph<sys::endpoint> graph;
		graph.add_nodes(hosts.begin(), hosts.end());
		#endif

		struct Handler {
			Handler(sys::fildes&& fd, sys::endpoint endp, int type):
			in(std::move(fd)), _endp(endp), _type(type)
			{ prefix(); }

			void
			prefix() {
				using namespace std::chrono;
				auto now = system_clock::now().time_since_epoch();
				auto millis = duration_cast<milliseconds>(now).count();
				buf << std::setw(15) << millis << ' ';
				buf << std::setw(15) << _endp.addr4() << ' ';
			}

			sys::fildes in;
			sys::endpoint _endp;
			int _type;
			std::stringstream buf;
		};
		sys::event_poller<std::shared_ptr<Handler>> poller;
		sys::process_group procs;
		int proc_number = 1;
		for (sys::endpoint endpoint : hosts) {
			sys::pipe opipe, epipe;
			opipe.in().unsetf(sys::fildes::non_blocking);
			opipe.out().unsetf(sys::fildes::non_blocking);
			epipe.in().unsetf(sys::fildes::non_blocking);
			epipe.out().unsetf(sys::fildes::non_blocking);
			procs.emplace([endpoint, &opipe, &epipe, proc_number, this] () {
                opipe.in().close();
                opipe.out().remap(STDOUT_FILENO);
                epipe.in().close();
                epipe.out().remap(STDERR_FILENO);
				uint32_t timeout = _testduration;
				if (_victims.contain(endpoint.addr4()) and kill_after != do_not_kill) {
					timeout = kill_after;
				}
				sys::ifaddr<sys::ipv4_addr> ifaddr(
					endpoint.addr4(),
					network.netmask()
				);
				sys::argstream command;
				if (ssh) {
					char workdir[PATH_MAX];
					::getcwd(workdir, PATH_MAX);
					command.append(
						"/usr/bin/ssh",
						"-n",
						"-o",
						"StrictHostKeyChecking no",
						endpoint.addr4(),
						"cd", workdir, ';', "exec"
					);
				}
				if (_netns) {
					std::stringstream netns_name;
					netns_name << _cluster << proc_number;
					command.append("/sbin/ip", "netns", "exec", netns_name.str());
					command.append("env");
					for (char** first=environ; *first; ++first) {
						command.append(*first);
					}
				}
				command.append(
					_argv[0],
					"--network", ifaddr,
					"--port", discovery_port,
					"--role", "slave",
					"--test-duration", timeout,
					"--master", _master
				);
				return sys::this_process::execute(
					const_cast<char* const*>(command.argv())
				);
			});
			opipe.out().close();
			epipe.out().close();
			sys::fd_type ofd = opipe.in().get_fd();
			poller.emplace(
				sys::poll_event{ofd, sys::poll_event::In},
				std::make_shared<Handler>(std::move(opipe.in()), endpoint, STDOUT_FILENO)
			);
			sys::fd_type efd = epipe.in().get_fd();
			poller.emplace(
				sys::poll_event{efd, sys::poll_event::In},
				std::make_shared<Handler>(std::move(epipe.in()), endpoint, STDERR_FILENO)
			);
			++proc_number;
		}

		#ifndef NDEBUG
		stdx::debug_message("tst", "forked _", procs);
		#endif

		volatile bool stopped = false;
		std::mutex mtx;
		std::thread thr1([&poller,&stopped,&mtx] () {
			std::unique_lock<std::mutex> lock(mtx);
			while (!stopped) {
				poller.wait(lock);
				stdx::unlock_guard<std::mutex> g(mtx);
				poller.for_each_ordinary_fd(
					[] (const sys::poll_event& ev, std::shared_ptr<Handler> handler) {
						typedef stdx::streambuf_traits<sys::fildes> traits_type;
						if (ev.in()) {
							const std::streamsize navail =
							traits_type::in_avail(handler->in);
							for (int i=0; i<navail; ++i) {
								char ch = 0;
								traits_type::read(handler->in, &ch, 1);
								handler->buf.put(ch);
								if (ch == '\n') {
									if (handler->_type == STDOUT_FILENO) {
										std::cout << handler->buf.rdbuf();
									} else {
										std::cerr << handler->buf.rdbuf();
									}
									handler->buf.clear();
									handler->prefix();
								}
							}
						}
					}
				);
			}
		});
		std::thread thr2([this,&procs,&stopped,&poller,&mtx] () {
			retval = procs.wait();
			std::unique_lock<std::mutex> lock(mtx);
			stopped = true;
			poller.notify_one();
		});
		thr1.join();
		thr2.join();

	}

	void
	run_slave(int argc, char* argv[]) {
		using namespace factory;
		factory::Terminate_guard g00;
		factory::Server_guard<decltype(local_server)> g1(local_server);
		factory::Server_guard<decltype(remote_server)> g2(remote_server);
		factory::Server_guard<decltype(timer_server)> g3(timer_server);
		local_server.send(new Main<sys::ipv4_addr>(argc, argv));
		retval = factory::wait_and_return();
	}

	void
	parse_command_line(int argc, char* argv[]) {
		sys::cmdline cmd(argc, argv, {
			sys::cmd::ignore_first_arg(),
			sys::cmd::make_option({"--hosts"}, hosts2),
			sys::cmd::make_option({"--network"}, network),
			sys::cmd::make_option({"--num-hosts"}, nhosts),
			sys::cmd::make_option({"--role"}, role),
			sys::cmd::make_option({"--port"}, discovery_port),
			sys::cmd::make_option({"--test-duration"}, _testduration),
			sys::cmd::make_option({"--master"}, _master),
			sys::cmd::make_option({"--victims"}, _victims),
			sys::cmd::make_option({"--kill-after"}, kill_after),
			sys::cmd::make_option({"--ssh"}, ssh),
			sys::cmd::make_option({"--netns"}, _netns),
			sys::cmd::make_option({"--cluster"}, _cluster)
		});
		cmd.parse();
		if (role != role_master and role != role_slave) {
			throw sys::invalid_cmdline_argument("--role");
		}
		if (!network) {
			throw sys::invalid_cmdline_argument("--network");
		}
		if (!_master) {
			_master = network.address();
		}
		if (_cluster.empty()) {
			_cluster = "cat";
		}
	}

};

int main(int argc, char* argv[]) {
	try {
		Fail_over_test test(argc, argv);
		test.run(argc, argv);
		return test.return_value();
	} catch (sys::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		return 1;
	}
}
