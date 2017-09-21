#ifndef BSCHEDULER_BASE_THREAD_NAME_HH
#define BSCHEDULER_BASE_THREAD_NAME_HH

namespace bsc {

	namespace this_thread {

		extern thread_local const char* name;
		extern thread_local unsigned number;

	}

}

#endif // vim:filetype=cpp
