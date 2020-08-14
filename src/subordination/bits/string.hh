#ifndef SUBORDINATION_BITS_STRING_HH
#define SUBORDINATION_BITS_STRING_HH

#include <string>

namespace sbn {

    namespace bits {

        inline void trim_right(std::string& s) {
            while (!s.empty() && std::isspace(s.back())) { s.pop_back(); }
        }

        inline void trim_left(std::string& s) {
            std::string::size_type i = 0;
            auto n = s.size();
            for (; i<n && std::isspace(s[i]); ++i) {}
            if (i != n) { s = s.substr(i); }
        }

        inline void trim_both(std::string& s) {
            trim_right(s);
            trim_left(s);
        }

    }

}

#endif // vim:filetype=cpp
