#ifndef SUBORDINATION_CORE_KERNEL_INSTANCE_REGISTRY_HH
#define SUBORDINATION_CORE_KERNEL_INSTANCE_REGISTRY_HH

#include <mutex>
#include <unordered_map>

#include <subordination/core/kernel.hh>

namespace sbn {

    class kernel_instance_registry {

    public:
        using kernel_type = kernel;
        using id_type = typename kernel_type::id_type;
        using kernel_table = std::unordered_map<id_type, kernel_type*>;
        using iterator = typename kernel_table::iterator;
        using const_iterator = typename kernel_table::const_iterator;
        using value_type = typename kernel_table::value_type;

    private:
        kernel_table _instances;
        mutable std::mutex _mutex;

    public:

        inline void
        lock() {
            this->_mutex.lock();
        }

        inline void
        unlock() {
            this->_mutex.unlock();
        }

        inline const_iterator
        begin() const noexcept {
            return this->_instances.begin();
        }

        inline const_iterator
        end() const noexcept {
            return this->_instances.end();
        }

        inline const_iterator
        find(id_type id) {
            return this->_instances.find(id);
        }

        inline void
        add(kernel_type* k) {
            if (!k->has_id()) { throw std::invalid_argument("bad id"); }
            this->_instances[k->id()] = k;
        }

        inline void
        free_instance(kernel_type* k) {
            this->_instances.erase(k->id());
        }

        friend std::ostream&
        operator<<(std::ostream& out, const kernel_instance_registry& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const kernel_instance_registry& rhs);

    using instance_registry_type = kernel_instance_registry;
    using instances_guard = std::lock_guard<instance_registry_type>;
    extern instance_registry_type instances;

}

#endif // vim:filetype=cpp
