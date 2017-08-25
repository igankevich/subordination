#include "factory.hh"
#include "config.hh"

factory::CPU_server<FACTORY_KERNEL_TYPE> factory::local_server;
factory::Timer_server<FACTORY_KERNEL_TYPE> factory::timer_server;
factory::IO_server<FACTORY_KERNEL_TYPE> factory::io_server;

