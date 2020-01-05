#ifndef SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH
#define SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/api.hh>

namespace sbn {

    class hierarchy_kernel: public sbn::kernel {

    public:
        typedef sys::ipv4_address addr_type;
        typedef sys::interface_address<addr_type> ifaddr_type;

    private:
        ifaddr_type _ifaddr;
        uint32_t _weight = 0;

    public:

        hierarchy_kernel() = default;

        inline
        hierarchy_kernel(const ifaddr_type& interface_address, uint32_t weight):
        _ifaddr(interface_address),
        _weight(weight)
        {}

        inline const ifaddr_type&
        interface_address() const noexcept {
            return this->_ifaddr;
        }

        inline void
        weight(uint32_t rhs) noexcept {
            this->_weight = rhs;
        }

        inline uint32_t
        weight() const noexcept {
            return this->_weight;
        }

        void
        write(sys::pstream& out) const override;

        void
        read(sys::pstream& in) override;

    };

}

#endif // vim:filetype=cpp
