#ifndef FACTORY_FACTORY_HH
#define FACTORY_FACTORY_HH

#include <factory/base/error_handler.hh>
#include <factory/kernel.hh>
#include <factory/kernel/algorithm.hh>
#include <factory/ppl/io_server.hh>
#include <factory/ppl/nic_server.hh>
#include <factory/ppl/server_guard.hh>
#include <factory/ppl/timer_server.hh>
#include <factory/registry.hh>

namespace factory {

	extern CPU_server<Kernel> local_server;
	extern Timer_server<Kernel> timer_server;
	extern IO_server<Kernel> io_server;

}

#endif // FACTORY_FACTORY_HH
