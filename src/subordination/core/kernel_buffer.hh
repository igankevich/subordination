#ifndef SUBORDINATION_CORE_KERNEL_BUFFER_HH
#define SUBORDINATION_CORE_KERNEL_BUFFER_HH

#include <unistdx/base/byte_buffer>

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/ipv6_address>
#include <unistdx/net/socket_address>

#include <subordination/core/types.hh>

namespace sbn {

    class kernel_buffer: public sys::byte_buffer {

    public:
        using sys::byte_buffer::byte_buffer;
        using sys::byte_buffer::read;
        using sys::byte_buffer::write;

        template <class T>
        auto write(T x) -> typename std::enable_if<std::is_enum<T>::value,void>::type {
            using type = typename std::underlying_type<T>::type;
            this->write(static_cast<type>(x));
        }

        template <class T>
        auto read(T& x) -> typename std::enable_if<std::is_enum<T>::value,void>::type {
            using type = typename std::underlying_type<T>::type;
            this->read(reinterpret_cast<type&>(x));
        }

        void write(const sys::socket_address& rhs);
        void read(sys::socket_address& rhs);
        void write(const sys::ipv4_address& rhs);
        void read(sys::ipv4_address& rhs);
        void write(const sys::ipv6_address& rhs);
        void read(sys::ipv6_address& rhs);
        template <class T>
        void write(const sys::interface_address<T>& rhs);
        template <class T>
        void read(sys::interface_address<T>& rhs);
        void write(foreign_kernel* k);
        void read(foreign_kernel* k);
        void write(kernel* k);
        void read(kernel*& k);

        template <class T> inline kernel_buffer&
        operator<<(const T& rhs) { this->write(rhs); return *this; }

        template <class T> inline kernel_buffer&
        operator>>(T& rhs) { this->read(rhs); return *this; }

    };

    class kernel_frame {

    public:
        using size_type = sys::u32;

    private:
        size_type _size = 0;

    public:
        inline void size(size_type rhs) noexcept { this->_size = rhs; }
        inline size_type size() const noexcept { return this->_size; }

    };

    class kernel_write_guard {

    private:
        kernel_frame& _frame;
        kernel_buffer& _buffer;
        sys::byte_buffer::size_type _old_position = 0;

    public:
        explicit kernel_write_guard(kernel_frame& frame, kernel_buffer& buffer);
        ~kernel_write_guard();

    };

    class kernel_read_guard {

    private:
        kernel_frame& frame;
        kernel_buffer& in;
        sys::byte_buffer::size_type _old_limit = 0;
        bool _good = false;

    public:
        explicit kernel_read_guard(kernel_frame& frame, kernel_buffer& buffer);
        ~kernel_read_guard();
        inline explicit operator bool() const noexcept { return this->_good; }
        inline bool operator!() const noexcept { return !this->_good; }

    };

}

#endif // vim:filetype=cpp
