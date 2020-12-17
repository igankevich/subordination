#ifndef SUBORDINATION_DAEMON_PROBE_HH
#define SUBORDINATION_DAEMON_PROBE_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/socket_address>

#include <subordination/core/kernel.hh>
#include <subordination/daemon/hierarchy_node.hh>

namespace sbnd {

    class probe: public sbn::kernel {

    public:
        using addr_type = sys::ipv4_address;
        using ifaddr_type = sys::interface_address<addr_type>;
        using hierarchy_node_array = std::vector<hierarchy_node>;

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _old_superior;
        sys::socket_address _new_superior;
        hierarchy_node_array _nodes;

    public:

        probe() = default;

        inline
        probe(
            const ifaddr_type& interface_address,
            const sys::socket_address& oldprinc,
            const sys::socket_address& newprinc
        ):
        _ifaddr(interface_address),
        _old_superior(oldprinc),
        _new_superior(newprinc)
        {}

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        inline const sys::socket_address& new_superior() const noexcept { return this->_new_superior; }
        inline const sys::socket_address& old_superior() const noexcept { return this->_old_superior; }
        inline const ifaddr_type& interface_address() const noexcept { return this->_ifaddr; }
        inline const hierarchy_node_array& nodes() const noexcept { return this->_nodes; }
        inline void nodes(const hierarchy_node_array& rhs) { this->_nodes = rhs; }
        inline void nodes(hierarchy_node_array&& rhs) { this->_nodes = std::move(rhs); }

    };

}

#endif // vim:filetype=cpp
