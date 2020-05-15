#ifndef SUBORDINATION_KERNEL_KERNEL_ERROR_HH
#define SUBORDINATION_KERNEL_KERNEL_ERROR_HH

#include <stdexcept>
#include <cstdint>
#include <iosfwd>

namespace sbn {

    class kernel_error: public std::runtime_error {

    public:
        typedef std::uintmax_t id_type;

    private:
        /// Identifier of the kernel or type.
        id_type _id;

    public:
        inline
        kernel_error(const char* msg, id_type id) noexcept:
        std::runtime_error(msg),
        _id(id)
        {}

        inline id_type
        id() const noexcept {
            return this->_id;
        }

        friend std::ostream&
        operator<<(std::ostream& out, const kernel_error& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const kernel_error& rhs);

}

#endif // vim:filetype=cpp
