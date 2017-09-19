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
	using namespace factory::api;
	using factory::network_master;
	factory::install_error_handler();
	factory::types.register_type<factory::Application_kernel>();
	factory::types.register_type<factory::probe>();
	factory::types.register_type<factory::hierarchy_kernel>();
	Factory_guard g;
	factory::factory.external().add_server(
		sys::endpoint(FACTORY_UNIX_DOMAIN_SOCKET)
	);
	network_master* m = new network_master;
	m->fanout(fanout);
	{
		factory::instances_guard g(factory::instances);
		factory::instances.add(m);
	}
	send<Local>(m);
	return factory::wait_and_return();
}
