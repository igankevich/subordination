#ifndef SUBORDINATION_KERNEL_FOREIGN_KERNEL_HH
#define SUBORDINATION_KERNEL_FOREIGN_KERNEL_HH

#include <subordination/kernel/kernel.hh>
#include <subordination/kernel/kernel_type.hh>

namespace sbn {

    class foreign_kernel: public kernel {

    private:
        typedef char char_type;
        typedef size_t size_type;
        typedef ::sbn::kernel_type::id_type id_type;

    private:
        size_type _size = 0;
        char_type* _payload = nullptr;
        id_type _type = 0;

    public:

        foreign_kernel() = default;

        foreign_kernel(const foreign_kernel&) = delete;

        foreign_kernel&
        operator=(const foreign_kernel&) = delete;

        inline
        ~foreign_kernel() {
            this->free();
        }

        inline id_type
        type() const noexcept {
            return this->_type;
        }

        void
        write(sys::pstream& out) const override;

        void
        read(sys::pstream& in) override;

    private:

        inline void
        free() {
            delete[] this->_payload;
            this->_payload = nullptr;
            this->_size = 0;
        }

    };

}

#endif // vim:filetype=cpp
