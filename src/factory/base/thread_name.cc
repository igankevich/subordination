#include "thread_name.hh"

thread_local const char* asc::this_thread::name = "<unknown>";
thread_local unsigned asc::this_thread::number = 0;
