#ifndef SUBORDINATION_CORE_KERNEL_BUFFER_HH
#define SUBORDINATION_CORE_KERNEL_BUFFER_HH

#include <chrono>
#include <vector>

#include <unistdx/base/byte_buffer>

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/ipv6_address>
#include <unistdx/net/socket_address>

#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class kernel_buffer: public sys::byte_buffer {

    private:
        kernel_type_registry* _types = nullptr;
        bool _carry_all_parents = false;

    public:
        using sys::byte_buffer::byte_buffer;
        using sys::byte_buffer::read;
        using sys::byte_buffer::write;

        template <class T>
        void write(const std::vector<T>& rhs) {
            const sys::u32 n = rhs.size();
            this->write(n);
            for (sys::u32 i=0; i<n; ++i) { *this << rhs[i]; }
        }

        template <class T>
        void read(std::vector<T>& rhs) {
            rhs.clear();
            sys::u32 n = 0;
            this->read(n);
            rhs.reserve(n);
            for (sys::u32 i=0; i<n; ++i) {
                rhs.emplace_back();
                *this >> rhs.back();
            }
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
        void write(const kernel* k);
        inline void write(const kernel_ptr& k) { this->write(k.get()); }
        void read(kernel_ptr& k);

        template <class T> inline kernel_buffer&
        operator>>(T& rhs) { this->read(rhs); return *this; }

        inline void types(kernel_type_registry* rhs) noexcept { this->_types = rhs; }
        inline const kernel_type_registry* types() const noexcept { return this->_types; }
        inline kernel_type_registry* types() noexcept { return this->_types; }
        inline void carry_all_parents(bool rhs) noexcept { this->_carry_all_parents = rhs; }
        inline bool carry_all_parents() const noexcept { return this->_carry_all_parents; }

    };

    template <class T> inline auto
    operator<<(kernel_buffer& out, const T& rhs)
    -> typename std::enable_if<!std::is_pointer<T>::value,kernel_buffer&>::type
    { out.write(rhs); return out; }

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
        kernel_frame& _frame;
        kernel_buffer& _buffer;
        sys::byte_buffer::size_type _old_limit = 0;
        bool _good = false;

    public:
        explicit kernel_read_guard(kernel_frame& frame, kernel_buffer& buffer);
        ~kernel_read_guard();
        inline bool good() const noexcept { return this->_good; }
        inline explicit operator bool() const noexcept { return good(); }
        inline bool operator!() const noexcept { return !good(); }

    };

}

#endif // vim:filetype=cpp
