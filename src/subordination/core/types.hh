#ifndef SUBORDINATION_CORE_TYPES_HH
#define SUBORDINATION_CORE_TYPES_HH

#include <cstdint>

namespace sbn {

    template<class Base>
    class basic_kernelbuf;
    class foreign_kernel;
    class kenel_type_error;
    class kernel;
    class kernel_base;
    class kernel_error;
    class kernel_header;
    class kernel_instance_registry;
    class kernel_type;
    class kernel_type_registry;
    class kstream;
    class mobile_kernel;
    enum class exit_code: std::uint16_t;
    enum class kernel_flag;

    class Factory;
    class application;
    class application_kernel;
    class connection;
    class basic_pipeline;
    class basic_socket_pipeline;
    class child_process_pipeline;
    class kernel_header_flag;
    class kernel_proto_flag;
    class kernel_protocol;
    class local_server;
    class parallel_pipeline;
    class pipeline;
    class pipeline_base;
    class process_handler;
    class process_pipeline;
    class remote_client;
    class socket_pipeline;
    class socket_pipeline_kernel;
    enum class process_role_type;
    enum class role_type;
    enum class socket_pipeline_event;

}

#endif // vim:filetype=cpp
