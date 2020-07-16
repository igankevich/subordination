#ifndef EXAMPLES_AUTOREG_VALARRAY_EXT_HH
#define EXAMPLES_AUTOREG_VALARRAY_EXT_HH

#include <iterator>
#include <list>
#include <ostream>
#include <sstream>
#include <string>
#include <valarray>
#include <vector>

namespace autoreg {

    template<class T>
    inline std::ostream&
    operator<<(std::ostream& out, std::valarray<T>& rhs) {
        std::copy(
            &rhs[0],
            &rhs[rhs.size()],
            std::ostream_iterator<T>(
                out,
                "\n"
            )
        );
        return out;
    }

    template<class T>
    inline std::ostream&
    operator<<(std::ostream& out, std::vector<T>& rhs) {
        std::copy(rhs.begin(), rhs.end(), std::ostream_iterator<T>(out, "\n"));
        return out;
    }

    template<class T>
    std::istream&
    operator>>(std::istream& stream, std::valarray<T>& rhs) {
        std::list<T> tmp;
        {
            std::string ln1;
            std::stringstream s;
            T val1;
            std::getline(stream, ln1);
            s << ln1;
            if (s >> val1 && s >> std::ws && s.eof()) {
                tmp.push_back(val1);
            }
        }
        std::istream_iterator<T> in_end, in(stream);
        std::copy(in, in_end, std::back_inserter(tmp));
        rhs.resize(tmp.size());
        std::copy(tmp.begin(), tmp.end(), &rhs[0]);
        return stream;
    }

    template<class T>
    inline std::ostream&
    operator<<(std::ostream& out, std::valarray< std::valarray<T> >& rhs) {
        for (size_t i=0; i<rhs.size(); ++i) {
            std::copy(
                &rhs[i][0],
                &rhs[i][rhs[i].size()],
                std::ostream_iterator<T>(out, " ")
            );
            out << std::endl;
        }
        return out;
    }

    template <class T> inline T
    mean(const std::valarray<T>& x) {
        return x.sum() / x.size();
    }

    template <class T> inline T
    variance(const std::valarray<T>& x) {
        return pow(x, T{2}).sum() / x.size();
    }

}

#endif // vim:filetype=cpp
