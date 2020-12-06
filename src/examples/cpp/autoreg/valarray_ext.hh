#ifndef EXAMPLES_CPP_AUTOREG_VALARRAY_EXT_HH
#define EXAMPLES_CPP_AUTOREG_VALARRAY_EXT_HH

#include <iterator>
#include <ostream>
#include <valarray>
#include <vector>

namespace autoreg {

    template <class T> inline std::ostream&
    operator<<(std::ostream& out, const std::valarray<T>& rhs) {
        std::copy_n(&rhs[0], rhs.size(), std::ostream_iterator<T>(out, "\n"));
        return out;
    }

    template <class T> inline std::ostream&
    operator<<(std::ostream& out, const std::vector<T>& rhs) {
        std::copy(rhs.begin(), rhs.end(), std::ostream_iterator<T>(out, "\n"));
        return out;
    }

    template <class T> inline T
    mean(const std::valarray<T>& x) {
        if (x.size() == 0) { return T{}; }
        return x.sum() / x.size();
    }

    template <class T> inline T
    variance(const std::valarray<T>& x) {
        if (x.size() == 0) { return T{}; }
        return pow(x, T{2}).sum() / x.size();
    }

}

#endif // vim:filetype=cpp
