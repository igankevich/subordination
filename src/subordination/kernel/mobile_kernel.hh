#ifndef SUBORDINATION_KERNEL_MOBILE_KERNEL_HH
#define SUBORDINATION_KERNEL_MOBILE_KERNEL_HH

#include <subordination/kernel/kernel_base.hh>
#include <subordination/kernel/kernel_header.hh>

namespace sbn {

    class mobile_kernel: public kernel_base, public kernel_header {

    public:
        typedef uint64_t id_type;

    private:
        id_type _id = no_id();

    public:
        virtual void
        read(sys::pstream& in);

        virtual void
        write(sys::pstream& out) const;

        inline id_type
        id() const noexcept {
            return this->_id;
        }

        inline void
        id(id_type rhs) noexcept {
            this->_id = rhs;
        }

        inline bool
        has_id() const noexcept {
            return this->_id != no_id();
        }

        inline void
        set_id(id_type rhs) noexcept {
            this->_id = rhs;
        }

        inline bool
        operator==(const mobile_kernel& rhs) const noexcept {
            return this == &rhs || (
                this->id() == rhs.id()
                && this->has_id()
                && rhs.has_id()
            );
        }

        inline bool
        operator!=(const mobile_kernel& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        static constexpr id_type
        no_id() noexcept {
            return 0;
        }

        inline uint64_t
        unique_id() const noexcept {
            return this->has_id() ? this->id() : uint64_t(this);
        }


    };

}

#endif // vim:filetype=cpp
