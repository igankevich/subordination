#ifndef SUBORDINATION_CORE_KERNEL_TYPE_REGISTRY_HH
#define SUBORDINATION_CORE_KERNEL_TYPE_REGISTRY_HH

#include <iosfwd>
#include <mutex>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_type.hh>
#include <subordination/core/kernel_type_error.hh>

namespace sbn {

    class kernel_type_registry {

    private:
        using kernel_type_array = std::vector<kernel_type>;

    public:
        using id_type = kernel_type::id_type;
        using iterator = kernel_type_array::iterator;
        using const_iterator = kernel_type_array::const_iterator;
        using value_type = kernel_type_array::value_type;
        using size_type = kernel_type_array::size_type;

    public:
        class sentry {
        private:
            const kernel_type_registry& _registry;
        public:
            inline explicit sentry(const kernel_type_registry& rhs): _registry(rhs) {
                this->_registry._mutex.lock();
            }
            inline ~sentry() { this->_registry._mutex.unlock(); }
        };

    private:
        kernel_type_array _types;
        mutable std::mutex _mutex;

    public:
        inline sentry guard() const { return sentry(*this); }
        inline const_iterator begin() const noexcept { return this->_types.begin(); }
        inline const_iterator end() const noexcept { return this->_types.end(); }
        inline size_type size() const noexcept { return this->_types.size(); }
        inline bool empty() const noexcept { return this->_types.empty(); }

        const_iterator find(id_type id) const noexcept;
        const_iterator find(std::type_index idx) const noexcept;
        void add(kernel_type type);

        template <class Type> void add(id_type id) {
            this->add(kernel_type{
                id,
                [] () -> kernel* { return new Type; },
                typeid(Type)
            });
        }

    };

    std::ostream& operator<<(std::ostream& out, const kernel_type_registry& rhs);

}

#endif // vim:filetype=cpp
