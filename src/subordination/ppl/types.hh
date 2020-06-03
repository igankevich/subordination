#ifndef SUBORDINATION_PPL_TYPES_HH
#define SUBORDINATION_PPL_TYPES_HH

namespace sbn {

    class application;
    class application_kernel;
    class basic_handler;
    template<class K, class T, class Th, class M, class L, class S> class basic_pipeline;
    class basic_socket_pipeline;
    class child_process_pipeline;
    class Factory;
    class io_pipeline;
    class kernel_header_flag;
    class kernel_protocol;
    class kernel_proto_flag;
    class local_server;
    class multi_pipeline;
    class parallel_pipeline;
    class pipeline;
    class pipeline_base;
    class process_handler;
    class process_pipeline;
    class remote_client;
    class socket_pipeline;
    class socket_pipeline_kernel;
    class timer_pipeline;
    enum class process_role_type;
    enum class role_type;
    enum class socket_pipeline_event;

}

#endif // vim:filetype=cpp
