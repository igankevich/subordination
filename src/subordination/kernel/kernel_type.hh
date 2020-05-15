#ifndef SUBORDINATION_KERNEL_KERNEL_TYPE_HH
#define SUBORDINATION_KERNEL_KERNEL_TYPE_HH

#include <typeinfo>
#include <typeindex>
#include <iosfwd>
#include <functional>

#include <unistdx/net/pstream>

#include <subordination/kernel/kernel.hh>

namespace sbn {

    class kernel_type {

    public:
        /// A portable type id
        typedef uint16_t id_type;
        typedef std::function<kernel* (sys::pstream&)> read_type;

    private:
        id_type _id;
        read_type _read;
        std::type_index _index;

    public:
        inline
        kernel_type(id_type id, const read_type& f, std::type_index idx) noexcept:
        _id(id),
        _read(f),
        _index(idx)
        {}

        inline
        kernel_type(const read_type& f, std::type_index idx) noexcept:
        _id(0),
        _read(f),
        _index(idx)
        {}

        inline kernel*
        read(sys::pstream& in) const {
            return this->_read(in);
        }

        inline std::type_index
        index() const noexcept {
            return this->_index;
        }

        inline id_type
        id() const noexcept {
            return this->_id;
        }

        inline void
        setid(id_type rhs) noexcept {
            this->_id = rhs;
        }

        inline const char*
        name() const noexcept {
            return this->_index.name();
        }

        inline explicit
        operator bool() const noexcept {
            return this->_id != 0;
        }

        inline bool
        operator !() const noexcept {
            return this->_id == 0;
        }

        inline bool
        operator==(const kernel_type& rhs) const noexcept {
            return this->_id == rhs._id;
        }

        inline bool
        operator!=(const kernel_type& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        inline bool
        operator<(const kernel_type& rhs) const noexcept {
            return this->_id < rhs._id;
        }

        friend std::ostream&
        operator<<(std::ostream& out, const kernel_type& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const kernel_type& rhs);

}

namespace std {

    template<>
    struct hash<sbn::kernel_type>: public std::hash<sbn::kernel_type::id_type> {

        typedef size_t result_type;
        typedef sbn::kernel_type argument_type;

        size_t
        operator()(const sbn::kernel_type& rhs) const noexcept {
            return std::hash<sbn::kernel_type::id_type>::operator()(rhs.id());
        }

    };

}

#endif // vim:filetype=cpp
