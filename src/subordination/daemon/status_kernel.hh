#ifndef SUBORDINATION_DAEMON_STATUS_KERNEL_HH
#define SUBORDINATION_DAEMON_STATUS_KERNEL_HH

#include <vector>

#include <subordination/daemon/hierarchy.hh>
#include <subordination/kernel/kernel.hh>

namespace sbn {

    class Status_kernel: public kernel {

    public:
        using address_type = sys::ipv4_address;
        using hierarchy_type = ::sbn::Hierarchy<address_type>;
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

        void write(sys::pstream& out) const override;
        void read(sys::pstream& in) override;

    };

}

#endif // vim:filetype=cpp
