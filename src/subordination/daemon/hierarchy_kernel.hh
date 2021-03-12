#ifndef SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH
#define SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/core/kernel.hh>
#include <subordination/daemon/hierarchy_node.hh>

namespace sbnd {

    class Hierarchy_kernel: public sbn::service_kernel {

    public:
        using addr_type = sys::ipv4_address;
        using ifaddr_type = sys::interface_address<addr_type>;
        using hierarchy_node_array = std::vector<hierarchy_node>;

    private:
        ifaddr_type _interface_address;
        hierarchy_node_array _nodes;

    public:

        Hierarchy_kernel() = default;

        inline
        Hierarchy_kernel(const ifaddr_type& interface_address,
                         const hierarchy_node_array& nodes):
        _interface_address(interface_address),
        _nodes(nodes)
        {}

        inline const ifaddr_type& interface_address() const noexcept { return this->_interface_address; }
        inline const hierarchy_node_array& nodes() const noexcept { return this->_nodes; }
        inline void nodes(const hierarchy_node_array& rhs) { this->_nodes = rhs; }

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    };

}

#endif // vim:filetype=cpp
