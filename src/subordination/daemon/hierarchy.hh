#ifndef SUBORDINATION_DAEMON_HIERARCHY_HH
#define SUBORDINATION_DAEMON_HIERARCHY_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/net/interface_address>
#include <unistdx/net/interface_socket_address>
#include <unistdx/net/socket_address>

#include <subordination/core/resources.hh>
#include <subordination/core/types.hh>
#include <subordination/daemon/hierarchy_node.hh>

namespace sbnd {

    template <class T>
    class Hierarchy {

    public:
        using addr_type = T;
        using interface_address_type = sys::interface_address<addr_type>;
        using interface_socket_address_type = sys::interface_socket_address<addr_type>;
        using container_type = std::unordered_map<sys::socket_address,hierarchy_node>;
        using const_iterator = typename container_type::const_iterator;
        using iterator = typename container_type::iterator;
        using size_type = typename container_type::size_type;
        using resource_array = sbn::resource_array;
        using hierarchy_node_array = std::vector<hierarchy_node>;
        using time_point = hierarchy_node::clock::time_point;

    protected:
        addr_type _netmask;
        hierarchy_node _this_node;
        container_type _nodes;

    public:

        inline explicit
        Hierarchy(const interface_address_type& ia, sys::port_type port) noexcept:
        _netmask(ia.netmask()) {
            std::clog << "port=" << port << std::endl;
            this->_this_node.socket_address(sys::ipv4_socket_address{ia.address(), port});
            std::clog << "this->_this_node.socket_address()=" << this->_this_node.socket_address() << std::endl;
            std::clog << "port()=" << this->port() << std::endl;
        }

        inline const container_type& nodes() const noexcept { return this->_nodes; }

        hierarchy_node_array nodes(int radius) const;
        hierarchy_node_array lower_nodes(const hierarchy_node& from, int depth) const;
        hierarchy_node_array upper_nodes(const hierarchy_node& from, int depth) const;

        inline hierarchy_node_array lower_nodes(int depth) const {
            return lower_nodes(this->_this_node, depth);
        }

        inline hierarchy_node_array upper_nodes(int depth) const {
            return upper_nodes(this->_this_node, depth);
        }

        inline hierarchy_node_array nodes_behind(const sys::socket_address& from) const {
            auto result = this->_nodes.find(from);
            if (result == this->_nodes.end()) { return {}; }
            if (from == superior_socket_address()) {
                return upper_nodes(result->second, std::numeric_limits<int>::max());
            } else {
                return lower_nodes(result->second, std::numeric_limits<int>::max());
            }
        }

        inline hierarchy_node_array subordinates() const { return lower_nodes(1); }

        /// Interface address of the current node.
        inline interface_address_type
        interface_address() const noexcept {
            const auto& sa = sys::socket_address_cast<sys::ipv4_socket_address>(
                this->_this_node.socket_address());
            return interface_address_type{sa.address(), this->_netmask};
        }

        /// Socket address of the current node.
        inline const sys::socket_address&
        socket_address() const noexcept {
            return this->_this_node.socket_address();
        }

        /// Interface address of the current node.
        inline interface_socket_address_type
        interface_socket_address() const noexcept {
            const auto& sa = sys::socket_address_cast<sys::ipv4_socket_address>(
                this->_this_node.socket_address());
            return interface_socket_address_type{sa.address(), this->_netmask, sa.port()};
        }

        inline addr_type netmask() const noexcept { return this->_netmask; }

        inline sys::port_type port() const noexcept {
            const auto& sa = sys::socket_address_cast<sys::ipv4_socket_address>(
                this->_this_node.socket_address());
            return sa.port();
        }

        inline const hierarchy_node& this_node() const noexcept { return this->_this_node; }

        /// Resources of the current node.
        inline const resource_array& resources() const noexcept {
            return this->_this_node.resources();
        }

        bool resources(const resource_array& rhs, time_point now);

        inline const sys::socket_address& superior_socket_address() const noexcept {
            return this->_this_node.superior_socket_address();
        }

        inline const hierarchy_node* superior() const noexcept {
            auto result = this->_nodes.find(this->_this_node.superior_socket_address());
            if (result == this->_nodes.end()) { return nullptr; }
            return &result->second;
        }

        bool add_nodes(const hierarchy_node_array& nodes, time_point now);
        bool add_superior(const hierarchy_node& node, time_point now);
        bool add_subordinate(const hierarchy_node& node, time_point now);
        bool remove_node(const sys::socket_address& sa, time_point now);

        inline bool
        has_superior() const noexcept {
            return static_cast<bool>(this->_this_node.superior_socket_address());
        }

        inline size_type num_neighbours() const noexcept { return this->_nodes.size(); }

        template <class X>
        friend std::ostream&
        operator<<(std::ostream& out, const Hierarchy<X>& rhs);

        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

        Hierarchy() = default;
        ~Hierarchy() = default;
        Hierarchy(const Hierarchy&) = default;
        Hierarchy& operator=(const Hierarchy&) = default;
        Hierarchy(Hierarchy&&) = default;
        Hierarchy& operator=(Hierarchy&&) = default;

    private:

        inline bool remove_node(const sys::socket_address& sa) {
            return this->_nodes.erase(sa) != 0;
        }

        template <class Predicate>
        inline void traverse(hierarchy_node_array& result, int j0, int radius,
                             Predicate pred) const {
            for (int i=0, j0=0, added=1; i<radius && added; ++i) {
                added = 0;
                const int j1 = result.size();
                for (int j=j0; j<j1; ++j) {
                    const auto& a = result[j];
                    for (const auto& pair : this->_nodes) {
                        const auto& b = pair.second;
                        if (pred(b, a)) {
                            result.emplace_back(b);
                            added = 1;
                        }
                    }
                }
                j0 = j1;
            }
        }

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
