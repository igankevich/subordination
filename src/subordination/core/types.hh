#ifndef SUBORDINATION_CORE_TYPES_HH
#define SUBORDINATION_CORE_TYPES_HH

#include <unistdx/base/types>

namespace sbn {

    class foreign_kernel;
    class kenel_type_error;
    class kernel;
    class kernel_base;
    class kernel_buffer;
    class kernel_error;
    class kernel_frame;
    class kernel_instance_registry;
    class kernel_read_guard;
    class kernel_type;
    class kernel_type_registry;
    class kernel_write_guard;
    enum class exit_code: sys::u16;
    enum class kernel_flag: sys::u32;

    class Factory;
    class application;
    class application_kernel;
    class basic_pipeline;
    class basic_socket_pipeline;
    class child_process_pipeline;
    class connection;
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
    enum class kernel_header_flag: sys::u8;
    enum class process_role_type;
    enum class role_type;
    enum class socket_pipeline_event;

}

#endif // vim:filetype=cpp
