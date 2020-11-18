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

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _old_superior;
        sys::socket_address _new_superior;
        hierarchy_node _superior;

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
        inline const hierarchy_node& superior() const noexcept { return this->_superior; }
        inline void superior(const hierarchy_node& rhs) noexcept { this->_superior = rhs; }

    };

}

#endif // vim:filetype=cpp
