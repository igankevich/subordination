#ifndef SUBORDINATION_CORE_THREAD_POOL_HH
#define SUBORDINATION_CORE_THREAD_POOL_HH

#include <thread>
#include <vector>

#include <unistdx/ipc/process>

namespace sbn {

    // TODO replace with sys::process
    class thread_pool: public std::vector<std::thread> {

    private:
        using base_type = std::vector<std::thread>;

    private:
        sys::cpu_set _cpus;

    public:
        using base_type::base_type;

        inline void join() { for (auto& t : *this) { if (t.joinable()) { t.join(); } } }
        inline void cpus(const sys::cpu_set& rhs) noexcept { this->_cpus = rhs; }
        inline const sys::cpu_set& cpus() const noexcept { return this->_cpus; }
        std::vector<int> cpu_array() const;

    };

}

#endif // vim:filetype=cpp
