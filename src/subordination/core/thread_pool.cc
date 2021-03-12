#include <subordination/core/thread_pool.hh>

std::vector<int> sbn::thread_pool::cpu_array() const {
    std::vector<int> cpus;
    cpus.reserve(size());
    auto count = this->_cpus.count();
    for (int i=0, j=0; j<count; ++i) {
        if (this->_cpus[i]) {
            cpus.emplace_back(i);
            ++j;
        }
    }
    return cpus;
}
