#ifndef SUBORDINATION_CORE_TYPES_HH
#define SUBORDINATION_CORE_TYPES_HH

#include <memory>
#include <vector>

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
    template <class T> class basic_weight_array;
    enum class exit_code: sys::u16;
    enum class kernel_flag: sys::u32;

    class Factory;
    class application;
    class basic_socket_pipeline;
    class child_process_pipeline;
    class connection;
    class parallel_pipeline;
    class pipeline;
    class pipeline_base;
    class process_handler;
    class transaction_log;
    enum class connection_flags: sys::u32;
    enum class kernel_header_flag: sys::u8;
    enum class process_role_type;
    enum class role_type;

    using kernel_ptr = std::unique_ptr<kernel>;
    using kernel_ptr_array = std::vector<kernel_ptr>;
    using foreign_kernel_ptr = std::unique_ptr<foreign_kernel>;
    using kernel_sack = std::vector<kernel_ptr>;
    //using weight_type = sys::u32;

    namespace resources {
        class Any;
        enum class Type: uint8_t;
        enum class resources: uint32_t;
        class Bindings;
        class Expression;
        class Symbol;
        class Constant;
        class Name;
        class Not;
        class Negate;
        class And;
        class Or;
        class Xor;
        class Less_than;
        class Less_or_equal;
        class Equal;
        class Greater_than;
        class Greater_or_equal;
        class Add;
        class Subtract;
        class Multiply;
        class Quotient;
        class Remainder;
        enum class Expressions: uint8_t;
        using expression_ptr = std::unique_ptr<Expression>;
    }

    using resource_array = resources::Bindings;

}

#endif // vim:filetype=cpp
