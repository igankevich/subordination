#ifndef SUBORDINATION_CORE_TYPES_HH
#define SUBORDINATION_CORE_TYPES_HH

#include <unistdx/base/types>

namespace sbn {

    class error;
    class foreign_kernel;
    class kernel;
    class kernel_base;
    class kernel_buffer;
    class kernel_frame;
    class kernel_instance_registry;
    class kernel_read_guard;
    class kernel_type;
    class kernel_type_registry;
    class kernel_write_guard;
    class type_error;
    enum class exit_code: sys::u16;
    enum class kernel_flag: sys::u32;

    class Factory;
    class application;
    class application_kernel;
    class basic_socket_pipeline;
    class child_process_pipeline;
    class connection;
    class parallel_pipeline;
    class pipeline;
    class pipeline_base;
    class process_handler;
    class transaction_log;
    enum class connection_flags: int;
    enum class kernel_header_flag: sys::u8;
    enum class process_role_type;
    enum class role_type;

}

#endif // vim:filetype=cpp
