#ifndef FACTORY_CFG_DAEMON_HH
#define FACTORY_CFG_DAEMON_HH

#include <factory/server/cpu_server.hh>
#include <factory/server/nic_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/app_server.hh>

namespace factory {

	inline namespace daemon_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sysx::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::Sub_Iserver<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}

}

#endif // FACTORY_CFG_DAEMON_HH
