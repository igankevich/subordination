#ifndef SUBORDINATION_DAEMON_BYTE_BUFFERS_HH
#define SUBORDINATION_DAEMON_BYTE_BUFFERS_HH

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <subordination/core/kernel_buffer.hh>

namespace std {

    template <class X, class Y> inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::pair<X,Y>& rhs) {
        out << rhs.first << rhs.second;
        return out;
    }

    template <class X, class Y> inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::pair<X,Y>& rhs) {
        in >> rhs.first >> rhs.second;
        return in;
    }

}


namespace sbnd {

    template <class T> inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::unordered_set<T>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
        return out;
    }

    template <class T> inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::unordered_set<T>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.reserve(n);
        T tmp;
        for (uint32_t i=0; i<n; ++i) {
            in >> tmp;
            rhs.emplace(tmp);
        }
        return in;
    }

    template <class X, class Y> inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::unordered_map<X,Y>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
        return out;
    }

    template <class X, class Y> inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::unordered_map<X,Y>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.reserve(n);
        std::pair<X,Y> tmp;
        for (uint32_t i=0; i<n; ++i) {
            in >> tmp;
            rhs.emplace(tmp);
        }
        return in;
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
