#ifndef SUBORDINATION_PPL_KERNEL_HEADER_FLAG_HH
#define SUBORDINATION_PPL_KERNEL_HEADER_FLAG_HH

#include <cstdint>

#include <unistdx/net/bstream>

namespace sbn {

    class kernel_header_flag {

    public:
        typedef std::uint8_t flag_type;

    private:
        flag_type _flag = 0;

    public:
        enum flag_enum: flag_type {
            has_source_and_destination = 1,
            has_application = 2,
            owns_application = 4,
            has_source = 8,
            has_destination = 16,
        };

        kernel_header_flag() = default;

        kernel_header_flag(const kernel_header_flag&) = default;

        inline
        kernel_header_flag(flag_type rhs) noexcept:
        _flag(rhs)
        {}

        inline
        operator flag_type() const noexcept {
            return this->_flag;
        }

        inline kernel_header_flag&
        operator|=(kernel_header_flag rhs) noexcept {
            this->_flag |= flag_type(rhs);
            return *this;
        }

        inline kernel_header_flag&
        operator&=(kernel_header_flag rhs) noexcept {
            this->_flag &= flag_type(rhs);
            return *this;
        }

        friend sys::bstream&
        operator<<(sys::bstream& out, const kernel_header_flag& rhs);

        friend sys::bstream&
        operator>>(sys::bstream& in, kernel_header_flag& rhs);

    };

    inline sys::bstream&
    operator<<(sys::bstream& out, const kernel_header_flag& rhs) {
        return out << rhs._flag;
    }

    inline sys::bstream&
    operator>>(sys::bstream& in, kernel_header_flag& rhs) {
        return in >> rhs._flag;
    }

    #define MAKE_UNARY(op) \
    inline kernel_header_flag \
    operator op(kernel_header_flag rhs) noexcept { \
        return op kernel_header_flag::flag_type(rhs); \
    }

    #define MAKE_BINARY(op, return_type) \
    inline return_type \
    operator op(kernel_header_flag lhs, kernel_header_flag rhs) noexcept { \
        return kernel_header_flag::flag_type(lhs) op kernel_header_flag::flag_type(rhs); \
    } \
    inline return_type \
    operator op(kernel_header_flag::flag_enum lhs, kernel_header_flag rhs) noexcept { \
        return kernel_header_flag::flag_type(lhs) op kernel_header_flag::flag_type(rhs); \
    } \
    inline return_type \
    operator op(kernel_header_flag lhs, kernel_header_flag::flag_enum rhs) noexcept { \
        return kernel_header_flag::flag_type(lhs) op kernel_header_flag::flag_type(rhs); \
    }

    MAKE_UNARY(~)
    MAKE_BINARY(|, kernel_header_flag)
    MAKE_BINARY(&, kernel_header_flag)
    MAKE_BINARY(^, kernel_header_flag)
    MAKE_BINARY(==, bool)
    MAKE_BINARY(!=, bool)

    #undef MAKE_UNARY
    #undef MAKE_BINARY

}

#endif // vim:filetype=cpp

