#ifndef SUBORDINATION_CORE_KERNEL_TYPE_REGISTRY_HH
#define SUBORDINATION_CORE_KERNEL_TYPE_REGISTRY_HH

#include <iosfwd>
#include <vector>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_type.hh>
#include <subordination/core/kernel_type_error.hh>

namespace sbn {

    class kernel_type_registry {

    public:
        typedef kernel_type::id_type id_type;
        typedef std::vector<kernel_type> container_type;
        typedef container_type::iterator iterator;
        typedef container_type::const_iterator const_iterator;

    private:
        container_type _types;
        id_type _counter = 0;

    public:
        const_iterator
        find(id_type id) const noexcept;

        const_iterator
        find(std::type_index idx) const noexcept;

        inline const_iterator
        begin() const noexcept {
            return this->_types.begin();
        }

        inline const_iterator
        end() const noexcept {
            return this->_types.end();
        }

        void
        register_type(kernel_type type);

        template <class Type> void
        register_type(id_type id) {
            this->register_type(kernel_type{
                id,
                [] () -> kernel* { return new Type; },
                typeid(Type)
            });
        }

        friend std::ostream&
        operator<<(std::ostream& out, const kernel_type_registry& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const kernel_type_registry& rhs);

    extern kernel_type_registry types;

    template <class Type>
    inline void
    register_type(kernel_type::id_type id) {
        types.register_type<Type>(id);
    }

    inline void
    register_type(const kernel_type& rhs) {
        types.register_type(rhs);
    }

}

#endif // vim:filetype=cpp
