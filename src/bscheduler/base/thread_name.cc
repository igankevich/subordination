#include "thread_name.hh"

thread_local const char* bsc::this_thread::name = "<unknown>";
thread_local unsigned bsc::this_thread::number = 0;
