#ifndef FACTORY_BASE_THREAD_NAME_HH
#define FACTORY_BASE_THREAD_NAME_HH

namespace asc {

	namespace this_thread {

		extern thread_local const char* name;
		extern thread_local unsigned number;

	}

}

#endif // vim:filetype=cpp
