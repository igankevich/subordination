#include <unistdx/net/endian>
#include <factory/api.hh>
#include <factory/ppl/application_kernel.hh>
#include <factory/base/error_handler.hh>

#include "network_master.hh"
#include "factory_socket.hh"

int main() {
	using namespace factory::api;
	using factory::network_master;
	{ sys::endian_guard g1; }
	factory::install_error_handler();
	factory::types.register_type<factory::Application_kernel>();
	factory::types.register_type<factory::probe>();
	Factory_guard g;
	factory::factory.external()
		.add_server(sys::endpoint(FACTORY_UNIX_DOMAIN_SOCKET));
	network_master* m = new network_master;
	{
		factory::instances_guard g(factory::instances);
		factory::instances.add(m);
	}
	send<Local>(m);
	return factory::wait_and_return();
}
