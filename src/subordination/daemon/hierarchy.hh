#ifndef SUBORDINATION_DAEMON_HIERARCHY_HH
#define SUBORDINATION_DAEMON_HIERARCHY_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/net/interface_address>
#include <unistdx/net/socket_address>

#include <subordination/core/types.hh>
#include <subordination/daemon/hierarchy_node.hh>

namespace sbnd {

    template <class T>
    class Hierarchy {

    public:
        using addr_type = T;
        using ifaddr_type = sys::interface_address<addr_type>;
        using container_type = std::unordered_map<sys::socket_address,hierarchy_node>;
        using const_iterator = typename container_type::const_iterator;
        using iterator = typename container_type::iterator;
        using size_type = typename container_type::size_type;
        using weight_type = hierarchy_node::weight_type;

    protected:
        ifaddr_type _ifaddr;
        sys::socket_address _socket_address;
        sys::socket_address _superior_socket_address;
        hierarchy_node _superior;
        container_type _subordinates;

    public:

        inline explicit
        Hierarchy(const ifaddr_type& interface_address, sys::port_type port) noexcept:
        _ifaddr(interface_address),
        _socket_address(sys::ipv4_socket_address{interface_address.address(), port})
        {}

        Hierarchy() = default;
        ~Hierarchy() = default;
        Hierarchy(const Hierarchy&) = default;
        Hierarchy& operator=(const Hierarchy&) = default;
        Hierarchy(Hierarchy&&) = default;
        Hierarchy& operator=(Hierarchy&&) = default;

        inline const ifaddr_type&
        interface_address() const noexcept {
            return this->_ifaddr;
        }

        inline const sys::socket_address&
        socket_address() const noexcept {
            return this->_socket_address;
        }

        inline const sys::socket_address& superior_socket_address() const noexcept {
            return this->_superior_socket_address;
        }

        inline const hierarchy_node&
        superior() const noexcept {
            return this->_superior;
        }

        inline bool
        remove_superior() noexcept {
            bool old_state = bool(this->_superior_socket_address);
            this->_superior_socket_address.clear();
            this->_superior.clear();
            return old_state != bool(this->_superior_socket_address);
        }

        inline bool
        add_superior(const sys::socket_address& addr, const hierarchy_node& node) {
            if (addr == this->_superior_socket_address &&
                node.weight() == this->_superior.weight()) { return false; }
            this->_superior_socket_address = addr;
            this->_superior = node;
            remove_subordinate(this->_superior_socket_address);
            return true;
        }

        inline void
        superior(const sys::socket_address& new_princ) {
            this->_superior_socket_address = new_princ;
            this->_superior.clear();
            remove_subordinate(this->_superior_socket_address);
        }

        bool add_subordinate(const sys::socket_address& addr);

        inline bool remove_subordinate(const sys::socket_address& addr) {
            return this->_subordinates.erase(addr) != 0;
        }

        inline size_type
        num_subordinates() const noexcept {
            return this->_subordinates.size();
        }

        inline bool
        has_superior() const noexcept {
            return static_cast<bool>(this->_superior_socket_address);
        }

        inline bool
        has_superior(const sys::socket_address& rhs) const noexcept {
            return this->_superior_socket_address == rhs;
        }

        inline bool
        has_subordinate(const sys::socket_address& rhs) const noexcept {
            return this->_subordinates.find(rhs) != this->_subordinates.end();
        }

        inline size_type
        num_neighbours() const noexcept {
            return this->num_subordinates() + (this->has_superior() ? 1 : 0);
        }

        inline sys::port_type
        port() const noexcept {
            return sys::ipaddr_traits<addr_type>::port(this->_socket_address);
        }

        inline const container_type& subordinates() const noexcept {
            return this->_subordinates;
        }

        inline bool
        set_superior(const hierarchy_node& new_superior) noexcept {
            if (this->_superior == new_superior) { return false; }
            this->_superior = new_superior;
            return true;
        }

        /// \brief Total weight of all subordinate and principal nodes and the current node.
        weight_type total_weight() const noexcept;

        /// @return total weight of all subordinate nodes
        weight_type total_subordinate_weight() const noexcept;

        inline weight_type
        superior_weight() const noexcept {
            return this->has_superior() ? this->_superior.weight() : 0;
        }

        bool
        set_subordinate(const sys::socket_address& endp, const hierarchy_node& node);

        template <class X>
        friend std::ostream&
        operator<<(std::ostream& out, const Hierarchy<X>& rhs);

        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

    };

    template <class X>
    std::ostream&
    operator<<(std::ostream& out, const Hierarchy<X>& rhs);

    template <class T> inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const Hierarchy<T>& rhs) {
        rhs.write(out); return out;
    }

    template <class T> inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, Hierarchy<T>& rhs) {
        rhs.read(in); return in;
    }

}


#endif // vim:filetype=cpp
