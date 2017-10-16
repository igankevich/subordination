#include <unistdx/base/cmdline>
#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <bscheduler/api.hh>
#include <bscheduler/base/error_handler.hh>
#include <bscheduler/ppl/application_kernel.hh>

#include "bscheduler_socket.hh"
#include "network_master.hh"

void
print_state(int) {
	std::clog << __func__ << std::endl;
	bsc::factory.print_state(std::clog);
}

void
install_debug_handler() {
	using namespace sys::this_process;
	bind_signal(sys::signal::quit, print_state);
}

int
main(int argc, char* argv[]) {
	#if defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
	{
		using namespace std::chrono;
		const auto now = system_clock::now().time_since_epoch();
		const auto t = duration_cast<milliseconds>(now);
		sys::log_message("discovery", "time since epoch _ms", t.count());
	}
	#endif
	using namespace bsc;
	sys::ipv4_addr::rep_type fanout = 10000;
	sys::ifaddr<sys::ipv4_addr> servers;
	bool allow_root = false;
	sys::input_operator_type options[] = {
		sys::ignore_first_argument(),
		sys::make_key_value("fanout", fanout),
		sys::make_key_value("servers", servers),
		sys::make_key_value("allow_root", allow_root),
		nullptr
	};
	sys::parse_arguments(argc, argv, options);
	install_error_handler();
	install_debug_handler();
	types.register_type<Application_kernel>();
	types.register_type<probe>();
	types.register_type<hierarchy_kernel>();
	factory_guard g;
	#if !defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
	factory.external().add_server(
		sys::endpoint(BSCHEDULER_UNIX_DOMAIN_SOCKET)
	);
	factory.child().allow_root(allow_root);
	#endif
	network_master* m = new network_master;
	m->allow(servers);
	m->fanout(fanout);
	{
		instances_guard g(instances);
		instances.add(m);
	}
	send<Local>(m);
	return wait_and_return();
}
