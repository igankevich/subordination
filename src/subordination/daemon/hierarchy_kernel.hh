#ifndef SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH
#define SUBORDINATION_DAEMON_HIERARCHY_KERNEL_HH

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/core/kernel.hh>
#include <subordination/core/resources.hh>

namespace sbnd {

    class Hierarchy_kernel: public sbn::kernel {

    public:
        using addr_type = sys::ipv4_address;
        using ifaddr_type = sys::interface_address<addr_type>;
        using resource_array = sbn::resources::Bindings;

    private:
        ifaddr_type _interface_address;
        resource_array _resources;

    public:

        Hierarchy_kernel() = default;

        inline
        Hierarchy_kernel(const ifaddr_type& interface_address,
                         const resource_array& resources):
        _interface_address(interface_address),
        _resources(resources)
        {}

        inline const ifaddr_type& interface_address() const noexcept { return this->_interface_address; }
        inline const resource_array& resources() const noexcept { return this->_resources; }
        inline void resources(const resource_array& rhs) { this->_resources = rhs; }

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    };

}

#endif // vim:filetype=cpp
