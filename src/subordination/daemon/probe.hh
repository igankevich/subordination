#ifndef SUBORDINATION_DAEMON_PROBE_HH
#define SUBORDINATION_DAEMON_PROBE_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/socket_address>

#include <subordination/core/kernel.hh>

namespace sbnd {

    class probe: public sbn::kernel {

    public:
        using addr_type = sys::ipv4_address;
        using ifaddr_type = sys::interface_address<addr_type>;
        using weight_type = uint32_t;

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _old_superior;
        sys::socket_address _new_superior;
        weight_type _superior_weight{};

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
        inline void superior_weight(weight_type rhs) noexcept { this->_superior_weight = rhs; }
        inline weight_type superior_weight() const noexcept { return this->_superior_weight; }

    };

}

#endif // vim:filetype=cpp
