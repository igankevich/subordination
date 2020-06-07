#ifndef SUBORDINATION_DAEMON_STATUS_KERNEL_HH
#define SUBORDINATION_DAEMON_STATUS_KERNEL_HH

#include <vector>

#include <subordination/core/kernel.hh>
#include <subordination/daemon/hierarchy.hh>

namespace sbnd {

    class Status_kernel: public sbn::kernel {

    public:
        using address_type = sys::ipv4_address;
        using hierarchy_type = Hierarchy<address_type>;
        using hierarchy_array = std::vector<hierarchy_type>;

    private:
        hierarchy_array _hierarchies;

    public:

        Status_kernel() = default;
        virtual ~Status_kernel() = default;

        inline void hierarchies(const hierarchy_array& rhs) {
            this->_hierarchies = rhs;
        }

        inline void hierarchies(hierarchy_array&& rhs) {
            this->_hierarchies = std::move(rhs);
        }

        inline const hierarchy_array& hierarchies() const noexcept {
            return this->_hierarchies;
        }

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

    };

}

#endif // vim:filetype=cpp
