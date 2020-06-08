#ifndef SUBORDINATION_CORE_KERNEL_TYPE_HH
#define SUBORDINATION_CORE_KERNEL_TYPE_HH

#include <functional>
#include <iosfwd>
#include <typeindex>
#include <typeinfo>

#include <subordination/core/kernel.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class kernel_type {

    public:
        /// A portable type id
        using id_type = uint16_t;
        using constructor_type = kernel* (*)(void*);

    private:
        id_type _id;
        constructor_type _construct;
        std::type_index _index;

    public:

        inline explicit
        kernel_type(id_type id, constructor_type ctr, std::type_index idx) noexcept:
        _id(id), _construct(ctr), _index(idx) {}

        inline explicit
        kernel_type(constructor_type ctr, std::type_index idx) noexcept:
        _id(0), _construct(ctr), _index(idx) {}

        inline kernel* construct(void* ptr) const { return this->_construct(ptr); }

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

        inline bool has_id() const noexcept { return this->_id != 0; }

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
