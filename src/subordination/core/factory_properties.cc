#include <sstream>

#include <unistdx/system/resource>

#include <subordination/core/factory_properties.hh>

namespace  {
    inline unsigned num_threads(const sys::cpu_set& available_cpus) {
        unsigned n = 1;
        const char* sbn_num_threads = std::getenv("SBN_NUM_THREADS");
        if (sbn_num_threads) {
            n = std::atoi(sbn_num_threads);
            if (n < 1) { n = 1; }
        } else {
            n = available_cpus.count();
        }
        return n;
    }
    inline unsigned num_downstream_threads() {
        unsigned n = 0;
        const char* sbn_num_threads = std::getenv("SBN_NUM_DOWNSTREAM_THREADS");
        if (sbn_num_threads) { n = std::atoi(sbn_num_threads); }
        return n;
    }
    template <class T> inline bool is_set(T value) {
        return value != std::numeric_limits<T>::max();
    }
    template <class Properties>
    inline bool set_if_prefix(const std::string& prefix,
                              Properties& properties,
                              const std::string& key,
                              const std::string& value) {
        if (key.compare(0, prefix.size(), prefix) == 0) {
            return properties.set(key.data() + prefix.size(), value);
        }
        return false;
    }
}

sbn::Properties::Properties() {
    if (const char* filename = std::getenv("SBN_CONFIG")) { open(filename); }
    const auto& available_cpus = sys::this_process::cpus();
    local.upstream_cpus &= available_cpus;
    if (local.upstream_cpus.count() == 0) {
        local.upstream_cpus = available_cpus;
    }
    local.downstream_cpus &= available_cpus;
    local.timer_cpus &= available_cpus;
    local.kernel_cpus &= available_cpus;
    if (!is_set(local.num_upstream_threads)) {
        local.num_upstream_threads = num_threads(local.upstream_cpus);
    }
    if (local.num_downstream_threads == 0) {
        local.num_downstream_threads = num_downstream_threads();
    }
    remote.cpus &= available_cpus;
}

void sbn::Properties::property(const std::string& key, const std::string& value) {
    if (set_if_prefix("local.", local, key, value)) {
    } else if (set_if_prefix("remote.", remote, key, value)) {
    } else {
        throw std::invalid_argument("unknown property");
    }
}
