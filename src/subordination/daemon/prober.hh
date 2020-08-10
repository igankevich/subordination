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
        using addr_type = sys::ipv4_address;
        using ifaddr_type = sys::interface_address<addr_type>;
        using weight_type = probe::weight_type;

    private:
        ifaddr_type _ifaddr;
        sys::socket_address _oldprinc;
        sys::socket_address _newprinc;
        weight_type _new_superior_weight{};
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
        new_superior() const noexcept {
            return this->_newprinc;
        }

        inline const sys::socket_address&
        old_superior() const noexcept {
            return this->_oldprinc;
        }

        inline weight_type new_superior_weight() const noexcept {
            return this->_new_superior_weight;
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
