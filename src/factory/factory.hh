#ifndef FACTORY_FACTORY_HH
#define FACTORY_FACTORY_HH

#if __cplusplus < 201103L
	#error Factory requires C++11 compiler.
#endif

#include <sys/process.hh>
#include <sys/log.hh>
#include <sys/packetstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

#include <factory/cpu_server.hh>
#include <factory/io_server.hh>
#include <factory/timer_server.hh>
#include <factory/nic_server.hh>
#include <factory/server_guard.hh>
#include <factory/reflection.hh>
#include <factory/algorithm.hh>

namespace factory {

	CPU_server<Kernel> local_server;
	Timer_server<Kernel> timer_server;
	IO_server<Kernel> io_server;

}

#endif // FACTORY_FACTORY_HH
