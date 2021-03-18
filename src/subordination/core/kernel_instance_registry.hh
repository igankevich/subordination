#ifndef SUBORDINATION_CORE_KERNEL_INSTANCE_REGISTRY_HH
#define SUBORDINATION_CORE_KERNEL_INSTANCE_REGISTRY_HH

#include <iosfwd>
#include <mutex>
#include <unordered_map>

#include <subordination/core/kernel.hh>

namespace sbn {

    class kernel_instance_registry {

    private:
        using kernel_table = std::unordered_map<kernel::id_type,kernel*>;

    public:
        using id_type = typename kernel::id_type;
        using iterator = typename kernel_table::iterator;
        using const_iterator = typename kernel_table::const_iterator;
        using value_type = typename kernel_table::value_type;
        using size_type = typename kernel_table::size_type;

    public:
        class sentry {
        private:
            const kernel_instance_registry& _registry;
        public:
            inline explicit sentry(const kernel_instance_registry& rhs): _registry(rhs) {
                this->_registry._mutex.lock();
            }
            inline ~sentry() { this->_registry._mutex.unlock(); }
        };

    private:
        kernel_table _instances;
        mutable std::mutex _mutex;

    public:
        inline sentry guard() const noexcept { return sentry(*this); }
        inline sentry guard() noexcept { return sentry(*this); }
        inline const_iterator begin() const noexcept { return this->_instances.begin(); }
        inline const_iterator end() const noexcept { return this->_instances.end(); }
        inline size_type size() const noexcept { return this->_instances.size(); }
        inline bool empty() const noexcept { return this->_instances.empty(); }
        inline const_iterator find(id_type id) { return this->_instances.find(id); }
        inline void remove(kernel* k) { this->_instances.erase(k->id()); }

        inline void
        add(kernel* k) {
            if (!k->has_id()) { throw std::invalid_argument("bad id"); }
            this->_instances[k->id()] = k;
        }

        void clear(kernel_sack& sack);

    };

    std::ostream& operator<<(std::ostream& out, const kernel_instance_registry& rhs);

}

#endif // vim:filetype=cpp
