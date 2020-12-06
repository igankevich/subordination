#ifndef EXAMPLES_CPP_AUTOREG_VECTOR_N_HH
#define EXAMPLES_CPP_AUTOREG_VECTOR_N_HH

#include <algorithm>
#include <cstdint>

#include <subordination/core/kernel_buffer.hh>

#if defined(AUTOREG_DEBUG)
#include <sstream>
#endif

namespace autoreg {

    template <class T, int N>
    class Vector {

    public:
        using value_type = T;
        static_assert(N > 0, "bad N");

    private:
        T _data[N] {};

    public:

        Vector() = default;
        ~Vector() = default;

        template <class A>
        inline Vector(const Vector<A,N>& v) {
            for (int i=0; i<N; ++i) { this->_data[i] = v[i]; }
        }

        template <class ... Args>
        inline explicit Vector(Args ... args): _data{static_cast<T>(args)...} {
            static_assert(sizeof...(args) == N, "bad no. of arguments");
        }

        inline Vector& operator=(const Vector& v) {
            for (int i=0; i<N; ++i) { this->_data[i] = v._data[i]; }
            return *this;
        }

        inline Vector& operator=(T rhs) {
            for (int i=0; i<N; ++i) { this->_data[i] = rhs; }
            return *this;
        }

        operator bool() = delete;
        bool operator !() = delete;

        inline void
        rot(int r) {
            std::rotate(&_data[0], &_data[r%N], &_data[N]);
        }

        inline T& operator[](int i) { return _data[i]; }
        inline const T& operator[](int i) const { return _data[i]; }

        inline T product() const {
            auto s = _data[0];
            for (int i=1; i<N; ++i) { s *= this->_data[i]; }
            return s;
        }

        template <class A, int m>
        friend std::ostream&
        operator<<(std::ostream& out, const Vector<A, m>& v);

        template <class A, int m>
        friend std::istream&
        operator>>(std::istream& in, Vector<A, m>& v);

        friend sbn::kernel_buffer&
        operator<<(sbn::kernel_buffer& out, const Vector& rhs) {
            for (int i=0; i<N; ++i) out << rhs._data[i];
            return out;
        }

        friend sbn::kernel_buffer&
        operator>>(sbn::kernel_buffer& in, Vector& rhs) {
            for (int i=0; i<N; ++i) in >> rhs._data[i];
            return in;
        }

        inline T* data() noexcept { return _data; }
        inline T* begin() noexcept { return _data; }
        inline T* end() noexcept { return _data + N; }
        inline const T* data() const noexcept { return _data; }
        inline const T* begin() const noexcept { return _data; }
        inline const T* end() const noexcept { return _data + N; }
        inline constexpr int size() noexcept { return N; }

        #define AUTOREG_OPERATOR_BINARY_MEMBER(OP) \
            inline Vector<T,N>& \
            operator OP(const Vector<T,N>& b) { \
                for (int i=0; i<N; ++i) { (*this)[i] OP b[i]; } \
                return *this; \
            } \
            template <class T2> \
            inline Vector<T,N>& \
            operator OP(T2 b) { \
                for (int i=0; i<N; ++i) { (*this)[i] OP b; } \
                return *this; \
            }

        AUTOREG_OPERATOR_BINARY_MEMBER(+=);
        AUTOREG_OPERATOR_BINARY_MEMBER(-=);
        AUTOREG_OPERATOR_BINARY_MEMBER(*=);
        AUTOREG_OPERATOR_BINARY_MEMBER(/=);
        AUTOREG_OPERATOR_BINARY_MEMBER(%=);
        AUTOREG_OPERATOR_BINARY_MEMBER(|=);
        AUTOREG_OPERATOR_BINARY_MEMBER(&=);
        AUTOREG_OPERATOR_BINARY_MEMBER(^=);

        #undef AUTOREG_OPERATOR_BINARY_MEMBER

    };

    #define AUTOREG_OPERATOR_BINARY(OP) \
        template <class T, int N> \
        inline Vector<T,N> \
        operator OP(const Vector<T,N>& a, const Vector<T,N>& b) { \
            Vector<T,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a[i] OP b[i]; } \
            return v; \
        } \
        template <class T, int N, class T2> \
        inline Vector<T,N> \
        operator OP(const Vector<T,N>& a, T2 b) { \
            Vector<T,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a[i] OP b; } \
            return v; \
        } \
        template <class T, int N, class T2> \
        inline Vector<T,N> \
        operator OP(T2 a, const Vector<T,N>& b) { \
            Vector<T,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a OP b[i]; } \
            return v; \
        }

    AUTOREG_OPERATOR_BINARY(+);
    AUTOREG_OPERATOR_BINARY(-);
    AUTOREG_OPERATOR_BINARY(*);
    AUTOREG_OPERATOR_BINARY(/);
    AUTOREG_OPERATOR_BINARY(%);
    AUTOREG_OPERATOR_BINARY(|);
    AUTOREG_OPERATOR_BINARY(&);
    AUTOREG_OPERATOR_BINARY(^);

    #undef AUTOREG_OPERATOR_BINARY

    #define AUTOREG_OPERATOR_BINARY_BOOL(OP) \
        template <class T, int N> \
        inline Vector<bool,N> \
        operator OP(const Vector<T,N>& a, const Vector<T,N>& b) { \
            Vector<bool,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a[i] OP b[i]; } \
            return v; \
        } \
        template <class T, int N, class T2> \
        inline Vector<bool,N> \
        operator OP(const Vector<T,N>& a, T2 b) { \
            Vector<bool,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a[i] OP b; } \
            return v; \
        } \
        template <class T, int N, class T2> \
        inline Vector<bool,N> \
        operator OP(T2 a, const Vector<T,N>& b) { \
            Vector<bool,N> v; \
            for (int i=0; i<N; ++i) { v[i] = a OP b[i]; } \
            return v; \
        }

    AUTOREG_OPERATOR_BINARY_BOOL(==);
    AUTOREG_OPERATOR_BINARY_BOOL(!=);
    AUTOREG_OPERATOR_BINARY_BOOL(<);
    AUTOREG_OPERATOR_BINARY_BOOL(>);
    AUTOREG_OPERATOR_BINARY_BOOL(<=);
    AUTOREG_OPERATOR_BINARY_BOOL(>=);

    #undef AUTOREG_OPERATOR_BINARY_BOOL

    template <class T, int N> inline Vector<T,N>
    min(const Vector<T,N>& a, const Vector<T,N>& b) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = std::min(a[i], b[i]); }
        return v;
    }

    template <class T, int N> inline Vector<T,N>
    max(const Vector<T,N>& a, const Vector<T,N>& b) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = std::max(a[i], b[i]); }
        return v;
    }

    template <class T, int N> inline Vector<T,N>
    where(const Vector<bool,N>& a, const Vector<T,N>& b, const Vector<T,N>& c) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = a[i] ? b[i] : c[i]; }
        return v;
    }

    template <class T, int N> inline Vector<T,N>
    where(const Vector<bool,N>& a, T b, const Vector<T,N>& c) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = a[i] ? b : c[i]; }
        return v;
    }

    template <class T, int N> inline Vector<T,N>
    where(const Vector<bool,N>& a, const Vector<T,N>& b, T c) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = a[i] ? b[i] : c; }
        return v;
    }

    template <class T, int N> inline Vector<T,N>
    where(const Vector<bool,N>& a, T b, T c) {
        Vector<T,N> v;
        for (int i=0; i<N; ++i) { v[i] = a[i] ? b : c; }
        return v;
    }

    using size3 = Vector<uint32_t, 3>;
    using size2 = Vector<uint32_t, 2>;
    using size1 = Vector<uint32_t, 1>;

    template<int N=3>
    class Index;

    template<>
    class Index<3> {

    private:
        size3 s;

    public:
        inline Index(const size3& sz): s(sz) {}
        inline int t(int i) const { return ((i/s[2])/s[1])%s[0]; }
        inline int x(int i) const { return (i/s[2])%s[1]; }
        inline int y(int i) const { return i%s[2]; }

        inline int operator()(int i, int j, int k) const
        #if !defined(AUTOREG_DEBUG)
        noexcept
        #endif
        {
            #if defined(AUTOREG_DEBUG)
            if (i < 0 || i >= int(s[0]) ||
                j < 0 || j >= int(s[1]) ||
                k < 0 || k >= int(s[2])) {
                std::stringstream tmp;
                tmp << "Out of bounds: index=" << size3(i,j,k) << ", size=" << s;
                throw std::runtime_error(tmp.str());
            }
            #endif
            return (i*s[1] + j)*s[2] + k;
        }

        inline int operator()(const size3& v) const noexcept {
            return operator()(v[0], v[1], v[2]);
        }

    };

    template<>
    class Index<1> {

    private:
        size1 s;

    public:
        inline Index(const size1& sz): s(sz) {}
        inline uint32_t operator()(uint32_t) const { return s[0]; }

    };


    template<>
    class Index<2> {

    private:
        size2 s;

    public:
        inline Index(const size2& sz): s(sz) {}
        inline uint32_t operator()(uint32_t i, uint32_t j) const { return i*s[1] + j; }

    };

    template <class A, int m>
    std::ostream&
    operator<<(std::ostream& out, const Vector<A, m>& v) {
        for (int i=0; i<m-1; ++i) out << v._data[i] << ',';
        return out << v._data[m-1];
    }

    template <class A, int m>
    std::istream&
    operator>>(std::istream& in, Vector<A, m>& v) {
        for (int i=0; i<m; ++i) { in >> v._data[i]; in.get(); }
        return in;
    }

}

#endif // vim:filetype=cpp
