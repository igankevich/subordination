#ifndef SUBORDINATION_DAEMON_HIERARCHY_NODE_HH
#define SUBORDINATION_DAEMON_HIERARCHY_NODE_HH

#include <iosfwd>

#include <unistdx/net/socket_address>

#include <subordination/core/types.hh>

namespace sbnd {

    class hierarchy_node {

    public:
        typedef uint32_t weight_type;

    private:
        sys::socket_address _socket_address;
        mutable weight_type _weight = 1;

    public:

        hierarchy_node() = default;

        inline explicit
        hierarchy_node(const sys::socket_address& socket_address):
        _socket_address(socket_address)
        {}

        inline
        hierarchy_node(const sys::socket_address& socket_address, weight_type w):
        _socket_address(socket_address),
        _weight(w)
        {}

        inline void
        reset() {
            this->_socket_address.reset();
            this->_weight = 0;
        }

        inline const sys::socket_address&
        socket_address() const noexcept {
            return this->_socket_address;
        }

        inline void
        socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline weight_type
        weight() const noexcept {
            return this->_weight;
        }

        inline void
        weight(weight_type rhs) const noexcept {
            this->_weight = rhs;
        }

        inline bool
        operator==(const hierarchy_node& rhs) const noexcept {
            return this->_socket_address == rhs._socket_address;
        }

        inline bool
        operator!=(const hierarchy_node& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        friend std::ostream&
        operator<<(std::ostream& out, const hierarchy_node& rhs);

        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

    };

    std::ostream&
    operator<<(std::ostream& out, const hierarchy_node& rhs);

    inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const hierarchy_node& rhs) {
        rhs.write(out); return out;
    }

    inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, hierarchy_node& rhs) {
        rhs.read(in); return in;
    }

    inline bool
    operator==(const hierarchy_node& lhs, const sys::socket_address& rhs) noexcept {
        return lhs.socket_address() == rhs;
    }

    inline bool
    operator==(const sys::socket_address& lhs, const hierarchy_node& rhs) noexcept {
        return lhs == rhs.socket_address();
    }

}

namespace std {

    template<>
    struct hash<sbnd::hierarchy_node>: public hash<sys::socket_address> {

        typedef size_t result_type;
        typedef sbnd::hierarchy_node argument_type;

        inline size_t
        operator()(const argument_type& rhs) const noexcept {
            return this->hash<sys::socket_address>::operator()(rhs.socket_address());
        }

    };

}

#endif // vim:filetype=cpp
