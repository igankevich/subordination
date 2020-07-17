#ifndef SUBORDINATION_DAEMON_PROBER_HH
#define SUBORDINATION_DAEMON_PROBER_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/socket_address>

#include <subordination/core/kernel.hh>
#include <subordination/daemon/probe.hh>

namespace sbnd {

    class prober: public sbn::kernel {

    public:
        typedef sys::ipv4_address addr_type;
        typedef sys::interface_address<addr_type> ifaddr_type;

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _oldprinc;
        sys::socket_address _newprinc;
        int _nprobes = 0;

    public:

        inline
        prober(
            const ifaddr_type& interface_address,
            const sys::socket_address& oldprinc,
            const sys::socket_address& newprinc
        ):
        _ifaddr(interface_address),
        _oldprinc(oldprinc),
        _newprinc(newprinc)
        {}

        void
        act() override;

        void
        react(sbn::kernel_ptr&& k) override;

        inline const sys::socket_address&
        new_principal() const noexcept {
            return this->_newprinc;
        }

        inline const sys::socket_address&
        old_principal() const noexcept {
            return this->_oldprinc;
        }

    private:

        void
        send_probe(const sys::socket_address& dest);

        template <class ... Args>
        inline void
        log(const char* fmt, const Args& ... args) {
            sys::log_message("prober", fmt, args ...);
        }

    };

}

#endif // vim:filetype=cpp
