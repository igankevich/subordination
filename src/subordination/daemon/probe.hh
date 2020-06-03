#ifndef SUBORDINATION_DAEMON_PROBE_HH
#define SUBORDINATION_DAEMON_PROBE_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/socket_address>

#include <subordination/kernel/kernel.hh>

namespace sbnd {

    class probe: public sbn::kernel {

    public:
        typedef sys::ipv4_address addr_type;
        typedef sys::interface_address<addr_type> ifaddr_type;

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _oldprinc;
        sys::socket_address _newprinc;

    public:

        probe() = default;

        inline
        probe(
            const ifaddr_type& interface_address,
            const sys::socket_address& oldprinc,
            const sys::socket_address& newprinc
        ):
        _ifaddr(interface_address),
        _oldprinc(oldprinc),
        _newprinc(newprinc)
        {}

        void
        write(sys::pstream& out) const override;

        void
        read(sys::pstream& in) override;

        inline const sys::socket_address&
        new_principal() const noexcept {
            return this->_newprinc;
        }

        inline const sys::socket_address&
        old_principal() const noexcept {
            return this->_oldprinc;
        }

        inline const ifaddr_type&
        interface_address() const noexcept {
            return this->_ifaddr;
        }

    };

}

#endif // vim:filetype=cpp
