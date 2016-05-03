
#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/io_server.hh>
#include <factory/server/nic_server.hh>

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sys::buffer_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::true_type {};

}


namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sys::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::IO_server<config> io_server;
		};

	}
}

using namespace factory;
using namespace factory::this_config;

#include "spec_app.hh"

int
main(int argc, char** argv) {
	return factory_main<Spec_app,config>(argc, argv);
}
