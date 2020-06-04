#ifndef SUBORDINATION_CORE_KERNEL_PROTO_FLAG_HH
#define SUBORDINATION_CORE_KERNEL_PROTO_FLAG_HH

namespace sbn {

    class kernel_proto_flag {

    public:
        typedef int flag_type;

    private:
        flag_type _flag = 0;

    public:
        enum flag_enum: flag_type {
            prepend_source_and_destination = 1,
            prepend_application = 2,
            save_upstream_kernels = 4,
            save_downstream_kernels = 8,
        };

        kernel_proto_flag() = default;

        kernel_proto_flag(const kernel_proto_flag&) = default;

        inline
        kernel_proto_flag(flag_type rhs) noexcept:
        _flag(rhs)
        {}

        inline
        operator flag_type() const noexcept {
            return this->_flag;
        }

        inline kernel_proto_flag&
        operator|=(kernel_proto_flag rhs) noexcept {
            this->_flag |= flag_type(rhs);
            return *this;
        }

        inline kernel_proto_flag&
        operator&=(kernel_proto_flag rhs) noexcept {
            this->_flag &= flag_type(rhs);
            return *this;
        }

    };

    #define MAKE_UNARY(op) \
    inline kernel_proto_flag \
    operator op(kernel_proto_flag rhs) noexcept { \
        return op kernel_proto_flag::flag_type(rhs); \
    }

    #define MAKE_BINARY(op, return_type) \
    inline return_type \
    operator op(kernel_proto_flag lhs, kernel_proto_flag rhs) noexcept { \
        return kernel_proto_flag::flag_type(lhs) op kernel_proto_flag::flag_type(rhs); \
    } \
    inline return_type \
    operator op(kernel_proto_flag::flag_type lhs, kernel_proto_flag rhs) noexcept { \
        return lhs op kernel_proto_flag::flag_type(rhs); \
    } \
    inline return_type \
    operator op(kernel_proto_flag lhs, kernel_proto_flag::flag_type rhs) noexcept { \
        return kernel_proto_flag::flag_type(lhs) op rhs; \
    }

    MAKE_UNARY(~)
    MAKE_BINARY(|, kernel_proto_flag)
    MAKE_BINARY(&, kernel_proto_flag)
    MAKE_BINARY(^, kernel_proto_flag)
    MAKE_BINARY(==, bool)
    MAKE_BINARY(!=, bool)

    #undef MAKE_UNARY
    #undef MAKE_BINARY

}

#endif // vim:filetype=cpp
