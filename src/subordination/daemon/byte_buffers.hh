#ifndef SUBORDINATION_DAEMON_BYTE_BUFFERS_HH
#define SUBORDINATION_DAEMON_BYTE_BUFFERS_HH

#include <unordered_set>
#include <vector>

#include <unistdx/net/pstream>

namespace sbnd {

    template <class T> inline void
    write_set(sys::pstream& out, const std::unordered_set<T>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
    }

    template <class T> inline void
    read_set(sys::pstream& in, std::unordered_set<T>& rhs) {
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

    template <class T> inline sys::pstream&
    operator<<(sys::pstream& out, const std::vector<T>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (const auto& v : rhs) { out << v; }
        return out;
    }

    template <class T> inline sys::pstream&
    operator>>(sys::pstream& in, std::vector<T>& rhs) {
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
