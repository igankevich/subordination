#include "thread_name.hh"

thread_local const char* factory::this_thread::name = "<unknown>";
thread_local unsigned factory::this_thread::number = 0;
