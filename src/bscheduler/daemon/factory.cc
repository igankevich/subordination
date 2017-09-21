#include <unistdx/base/cmdline>

#include <bscheduler/api.hh>
#include <bscheduler/base/error_handler.hh>
#include <bscheduler/ppl/application_kernel.hh>

#include "factory_socket.hh"
#include "network_master.hh"

int
main(int argc, char* argv[]) {
	sys::ipv4_addr::rep_type fanout = 10000;
	sys::input_operator_type options[] = {
		sys::ignore_first_argument(),
		sys::make_key_value("fanout", fanout),
		nullptr
	};
	sys::parse_arguments(argc, argv, options);
	bsc::install_error_handler();
	bsc::types.register_type<bsc::Application_kernel>();
	bsc::types.register_type<bsc::probe>();
	bsc::types.register_type<bsc::hierarchy_kernel>();
	bsc::factory_guard g;
	bsc::factory.external().add_server(
		sys::endpoint(BSCHEDULER_UNIX_DOMAIN_SOCKET)
	);
	bsc::network_master* m = new bsc::network_master;
	m->fanout(fanout);
	{
		bsc::instances_guard g(bsc::instances);
		bsc::instances.add(m);
	}
	bsc::send<bsc::Local>(m);
	return bsc::wait_and_return();
}
