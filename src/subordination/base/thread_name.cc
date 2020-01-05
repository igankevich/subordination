#include "thread_name.hh"

thread_local const char* sbn::this_thread::name = "<unknown>";
thread_local unsigned sbn::this_thread::number = 0;
