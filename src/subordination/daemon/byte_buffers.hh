#ifndef SUBORDINATION_DAEMON_BYTE_BUFFERS_HH
#define SUBORDINATION_DAEMON_BYTE_BUFFERS_HH

#include <unordered_set>
#include <vector>

#include <subordination/core/kernel_buffer.hh>

namespace sbnd {

    template <class T> inline void
    write_set(sbn::kernel_buffer& out, const std::unordered_set<T>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
    }

    template <class T> inline void
    read_set(sbn::kernel_buffer& in, std::unordered_set<T>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.reserve(n);
        T tmp;
        for (uint32_t i=0; i<n; ++i) {
            in >> tmp;
            rhs.emplace(tmp);
        }
    }

    template <class T> inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::vector<T>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
        return out;
    }

    template <class T> inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::vector<T>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.reserve(n);
        T tmp;
        for (uint32_t i=0; i<n; ++i) {
            in >> tmp;
            rhs.emplace_back(tmp);
        }
        return in;
    }

}

#endif // vim:filetype=cpp
