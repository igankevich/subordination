#include <unistdx/base/cmdline>

#include <factory/api.hh>
#include <factory/base/error_handler.hh>
#include <factory/ppl/application_kernel.hh>

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
	asc::install_error_handler();
	asc::types.register_type<asc::Application_kernel>();
	asc::types.register_type<asc::probe>();
	asc::types.register_type<asc::hierarchy_kernel>();
	asc::factory_guard g;
	asc::factory.external().add_server(
		sys::endpoint(FACTORY_UNIX_DOMAIN_SOCKET)
	);
	asc::network_master* m = new asc::network_master;
	m->fanout(fanout);
	{
		asc::instances_guard g(asc::instances);
		asc::instances.add(m);
	}
	asc::send<asc::Local>(m);
	return asc::wait_and_return();
}
