#ifndef SUBORDINATION_DAEMON_TYPES_HH
#define SUBORDINATION_DAEMON_TYPES_HH

#include <unistdx/base/types>

namespace sbnd {

    class Foreign_main_kernel;
    class Hierarchy_kernel;
    class Job_status_kernel;
    class Pipeline_status_kernel;
    class Status_kernel;
    class Terminate_kernel;
    class Transaction_gather_subordinate;
    class Transaction_gather_superior;
    class Transaction_test_kernel;
    class local_server;
    class probe;
    class process_pipeline;
    class process_pipeline_kernel;
    class socket_pipeline;
    class socket_pipeline_client;
    class socket_pipeline_kernel;
    class unix_socket_client;
    class unix_socket_pipeline;
    class unix_socket_server;
    enum class process_pipeline_event: sys::u8;
    enum class socket_pipeline_event;

}

#endif // vim:filetype=cpp
