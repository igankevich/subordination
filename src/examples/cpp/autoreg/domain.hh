#ifndef EXAMPLES_CPP_AUTOREG_DOMAIN_HH
#define EXAMPLES_CPP_AUTOREG_DOMAIN_HH

#include <istream>
#include <ostream>

#include <autoreg/vector_n.hh>

namespace autoreg {

    template <class T, size_t D=3u, class S=size_t>
    class Domain {
    public:
        typedef Vector<T, D> Vec;
        typedef Vector<S, D> Size;

        Domain(Vec mi, Vec ma, Size s):
        lower(mi),
        upper(ma),
        length(s)
        {}

        Domain(Vec d, Size s):
        lower(), upper(d*Vec(s)), length(s) {}

        Domain(size_t nx, size_t ny, size_t nt):
        lower(0, 0, 0), upper(nx-1, ny-1, nt-1), length(nx, ny, nt) {}

        Domain(Size s):
        lower(), upper(s), length(s) {}
        Domain(
            T x0,
            T x1,
            size_t i,
            T y0,
            T y1,
            size_t j,
            T t0,
            T t1,
            size_t
            k
        ):
        lower(x0, y0, t0), upper(x1, y1, t1), length(i, j, k) {}

        Domain():
        lower(), upper(), length(S(1)) {}

        template <class A>
        Domain(const Domain<A, D>& d):
        lower(d.lower), upper(d.upper), length(d.length) {}

        static constexpr inline size_t dimensions() noexcept { return D; }
        inline const Vec& min() const noexcept { return lower; }
        inline const Vec& max() const noexcept { return upper; }
        inline const Size& count() const noexcept { return length; }
        inline Vec& min() noexcept { return lower; }
        inline Vec& max() noexcept { return upper; }
        inline Size& count() noexcept { return length; }
        inline size_t size() const noexcept { return length; }
        inline Vec delta() const { return (upper-lower)/Vec(length); }
        inline Vec point(Size idx) const { return lower + delta() * Vec(idx); }

        template <class A, size_t B, class C>
        inline const Domain<T, D, S>&
        operator=(const Domain<A, B, C>& d) {
            lower = d.lower; upper = d.upper; length = d.length; return *this;
        }

        template <class A, size_t B, class C>
        friend std::ostream&
        operator<<(std::ostream&, const Domain<A, B, C>&);

        template <class A, size_t B, class C>
        friend std::istream&
        operator>>(std::istream&, Domain<A, B, C>&);

    private:
        Vec lower, upper;
        Size length;
    };

    template <class T, size_t D, class S>
    Domain<T, D, S>
    operator*(const Domain<T, D, S>& d, T factor) {
        return Domain<T, D, S>(
            d.lower,
            d.lower+(d.upper-d.lower)*factor,
            d.length*factor
        );
    }

    template <class T, size_t D, class S>
    Domain<T, D, S>
    operator+(const Domain<T, D, S>& d, size_t addon) {
        return Domain<T, D, S>(
            d.lower,
            d.lower+((d.upper-d.lower)/d.length)*
            (d.length+addon),
            d.length+addon
        );
    }

    template <class T, size_t D, class S>
    std::ostream&
    operator<<(std::ostream& out, const Domain<T, D, S>& d) {
        return out << d.lower << '\n' << d.upper << '\n' << d.length;
    }

    template <class T, size_t D, class S>
    std::istream&
    operator>>(std::istream& in, Domain<T, D, S>& d) {
        return in >> d.lower >> d.upper >> d.length;
    }

}

#endif // vim:filetype=cpp
