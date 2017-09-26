#include <unistdx/base/cmdline>
#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>
#if defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
#include <unistdx/ipc/signal>
#endif

#include <bscheduler/api.hh>
#include <bscheduler/base/error_handler.hh>
#include <bscheduler/ppl/application_kernel.hh>

#include "factory_socket.hh"
#include "network_master.hh"

#if defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
void
print_end_marker(int) {
	sys::log_message("discovery", "terminate");
	std::quick_exit(0);
}
#endif

int
main(int argc, char* argv[]) {
	using namespace bsc;
	sys::ipv4_addr::rep_type fanout = 10000;
	sys::ifaddr<sys::ipv4_addr> servers;
	sys::input_operator_type options[] = {
		sys::ignore_first_argument(),
		sys::make_key_value("fanout", fanout),
		sys::make_key_value("servers", servers),
		nullptr
	};
	sys::parse_arguments(argc, argv, options);
	install_error_handler();
	#if defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
	sys::this_process::bind_signal(sys::signal::terminate, print_end_marker);
	#endif
	types.register_type<Application_kernel>();
	types.register_type<probe>();
	types.register_type<hierarchy_kernel>();
	factory_guard g;
	#if !defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
	factory.external().add_server(
		sys::endpoint(BSCHEDULER_UNIX_DOMAIN_SOCKET)
	);
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
