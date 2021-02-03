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
}

sbn::Properties::Properties() {
    if (const char* filename = std::getenv("SBN_CONFIG")) { open(filename); }
    const auto& available_cpus = sys::this_process::cpu_affinity();
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
    if (!is_set(remote.min_input_buffer_size)) {
        remote.min_input_buffer_size = sys::page_size()*16;
    }
    if (!is_set(remote.min_output_buffer_size)) {
        remote.min_output_buffer_size = sys::page_size()*16;
    }
    remote.cpus &= available_cpus;
}

void sbn::Properties::property(const std::string& key, const std::string& value) {
    if (key == "local.num-upstream-threads") {
        auto n = std::stoi(value);
        if (n < 1) { throw std::out_of_range("out of range"); }
        local.num_upstream_threads = n;
    } else if (key == "local.num-downstream-threads") {
        auto n = std::stoi(value);
        if (n < 0) { throw std::out_of_range("out of range"); }
        local.num_downstream_threads = n;
    } else if (key == "local.upstream-cpus") {
        std::stringstream tmp(value);
        tmp >> local.upstream_cpus;
        if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
    } else if (key == "local.downstream-cpus") {
        std::stringstream tmp(value);
        tmp >> local.downstream_cpus;
        if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
    } else if (key == "local.timer-cpus") {
        std::stringstream tmp(value);
        tmp >> local.timer_cpus;
        if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
    } else if (key == "local.kernel-cpus") {
        std::stringstream tmp(value);
        tmp >> local.kernel_cpus;
        if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
    } else if (key == "remote.min-input-buffer-size") {
        auto n = std::stoul(value);
        remote.min_input_buffer_size = n;
    } else if (key == "remote.min-output-buffer-size") {
        auto n = std::stoul(value);
        remote.min_output_buffer_size = n;
    } else if (key == "remote.pipe-buffer-size") {
        auto n = std::stoul(value);
        remote.pipe_buffer_size = n;
    } else if (key == "remote.cpus") {
        std::stringstream tmp(value);
        tmp >> remote.cpus;
        if (!tmp) { throw std::invalid_argument("bad cpu mask"); }
    } else {
        throw std::invalid_argument("unknown property");
    }
}
