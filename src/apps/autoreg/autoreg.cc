#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sys::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
		};

	}
}

using namespace factory;
using namespace factory::this_config;

#include "autoreg_app.hh"

int
main(int argc, char** argv) {
	return factory_main<Autoreg_app,config>(argc, argv);
}
