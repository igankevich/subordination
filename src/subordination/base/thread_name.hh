#ifndef SUBORDINATION_BASE_THREAD_NAME_HH
#define SUBORDINATION_BASE_THREAD_NAME_HH

namespace sbn {

    namespace this_thread {

        extern thread_local const char* name;
        extern thread_local unsigned number;

    }

}

#endif // vim:filetype=cpp
