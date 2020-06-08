#ifndef SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH
#define SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/core/kernel.hh>

namespace sbnd {

    class Hierarchy_kernel: public sbn::kernel {

    public:
        typedef sys::ipv4_address addr_type;
        typedef sys::interface_address<addr_type> ifaddr_type;

    private:
        ifaddr_type _ifaddr;
        uint32_t _weight = 0;

    public:

        Hierarchy_kernel() = default;

        inline
        Hierarchy_kernel(const ifaddr_type& interface_address, uint32_t weight):
        _ifaddr(interface_address),
        _weight(weight)
        {}

        inline const ifaddr_type& interface_address() const noexcept { return this->_ifaddr; }
        inline void weight(uint32_t rhs) noexcept { this->_weight = rhs; }
        inline uint32_t weight() const noexcept { return this->_weight; }

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    };

}

#endif // vim:filetype=cpp