#ifndef SUBORDINATION_CORE_WEIGHTS_HH
#define SUBORDINATION_CORE_WEIGHTS_HH

#include <array>
#include <cstdint>
#include <ostream>

#include <subordination/core/types.hh>

namespace sbn {

    template <class T>
    class basic_weight {

        static_assert(std::is_unsigned<T>::value, "expect unsigned integral type");

    private:
        T _value{};

    public:

        constexpr inline basic_weight(T value) noexcept: _value{value} {};

        basic_weight() = default;
        ~basic_weight() = default;
        basic_weight(const basic_weight&) = default;
        basic_weight& operator=(const basic_weight&) = default;
        basic_weight(basic_weight&&) = default;
        basic_weight& operator=(basic_weight&&) = default;

        //inline basic_weight& operator=(T value) noexcept { this->_value = value; return *this; }

        inline basic_weight& operator+=(T b) noexcept {
            if (get() < std::numeric_limits<T>::max()-b) {
                this->_value += b;
            } else {
                this->_value = std::numeric_limits<T>::max();
            }
            return *this;
        }

        inline basic_weight& operator-=(T b) noexcept {
            if (get() > std::numeric_limits<T>::lowest() + b) {
                this->_value -= b;
            } else {
                this->_value = std::numeric_limits<T>::lowest();
            }
            return *this;
        }

        inline basic_weight& operator*=(T b) noexcept {
            if (get() < std::numeric_limits<T>::max()/b +
                        (std::numeric_limits<T>::max()%b == 0 ? 0 : 1)) {
                this->_value *= b;
            } else {
                this->_value = std::numeric_limits<T>::max();
            }
            return *this;
        }

        inline basic_weight& operator/=(T b) noexcept {
            if (b == T{}) {
                this->_value = std::numeric_limits<T>::max();
            } else {
                this->_value /= b;
            }
            return *this;
        }

        inline basic_weight& operator%=(T b) noexcept {
            if (b == T{}) {
                this->_value = T{};
            } else {
                this->_value %= b;
            }
            return *this;
        }

        #define SBN_INTERNAL_UNWRAP(OP) \
            inline basic_weight& operator OP(basic_weight b) noexcept { \
                return operator OP(b.get()); \
            }

        SBN_INTERNAL_UNWRAP(+=);
        SBN_INTERNAL_UNWRAP(-=);
        SBN_INTERNAL_UNWRAP(*=);
        SBN_INTERNAL_UNWRAP(/=);
        SBN_INTERNAL_UNWRAP(%=);

        #undef SBN_INTERNAL_UNWRAP

        constexpr inline T get() const noexcept { return this->_value; }
        inline T& get() noexcept { return this->_value; }

        friend inline std::ostream& operator<<(std::ostream& out, const basic_weight<T>& rhs) {
            return out << rhs._value;
        }

        friend inline std::istream& operator>>(std::istream& in, basic_weight<T>& rhs) {
            return in >> rhs._value;
        }

    };

    template <class T>
    inline basic_weight<T> operator+(basic_weight<T> a, basic_weight<T> b) noexcept {
        if (a.get() < std::numeric_limits<T>::max()-b.get()) {
            return basic_weight<T>{a.get() + b.get()};
        }
        return basic_weight<T>{std::numeric_limits<T>::max()};
    }

    template <class T>
    inline basic_weight<T> operator-(basic_weight<T> a, basic_weight<T> b) noexcept {
        if (a.get() > std::numeric_limits<T>::lowest() + b.get()) {
            return basic_weight<T>{a.get() - b.get()};
        }
        return basic_weight<T>{std::numeric_limits<T>::lowest()};
    }

    template <class T>
    inline basic_weight<T> operator*(basic_weight<T> a, basic_weight<T> b) noexcept {
        if (a.get() < std::numeric_limits<T>::max()/b.get() +
                      (std::numeric_limits<T>::max()%b.get() == 0 ? 0 : 1)) {
            return basic_weight<T>{a.get() * b.get()};
        }
        return basic_weight<T>{std::numeric_limits<T>::max()};
    }

    template <class T>
    inline basic_weight<T> operator/(basic_weight<T> a, basic_weight<T> b) noexcept {
        if (b.get() == T{}) { return basic_weight<T>{std::numeric_limits<T>::max()}; }
        return basic_weight<T>{a.get() / b.get()};
    }

    template <class T>
    inline basic_weight<T> operator%(basic_weight<T> a, basic_weight<T> b) noexcept {
        if (b.get() == T{}) { return basic_weight<T>{T{}}; };
        return basic_weight<T>{a.get() % b.get()};
    }

    #define SBN_INTERNAL_WRAP(OP) \
        template <class T> \
        inline basic_weight<T> operator OP(basic_weight<T> a, T b) noexcept { \
            return operator OP(a, basic_weight<T>{b}); \
        } \
        template <class T> \
        inline basic_weight<T> operator OP(T a, basic_weight<T> b) noexcept { \
            return operator OP(basic_weight<T>{a}, b); \
        }

    SBN_INTERNAL_WRAP(+);
    SBN_INTERNAL_WRAP(-);
    SBN_INTERNAL_WRAP(*);
    SBN_INTERNAL_WRAP(/);
    SBN_INTERNAL_WRAP(%);

    #undef SBN_INTERNAL_WRAP

    #define SBN_INTERNAL_UNWRAP(OP) \
        template <class T> \
        constexpr inline bool operator OP(basic_weight<T> a, T b) noexcept { \
            return a.get() OP basic_weight<T>{b}; \
        } \
        template <class T> \
        constexpr inline bool operator OP(T a, basic_weight<T> b) noexcept { \
            return a OP b.get(); \
        } \
        template <class T> \
        constexpr inline bool operator OP(basic_weight<T> a, basic_weight<T> b) noexcept { \
            return a.get() OP b.get(); \
        }
    SBN_INTERNAL_UNWRAP(<);
    SBN_INTERNAL_UNWRAP(<=);
    SBN_INTERNAL_UNWRAP(>=);
    SBN_INTERNAL_UNWRAP(>);
    SBN_INTERNAL_UNWRAP(==);
    SBN_INTERNAL_UNWRAP(!=);
    #undef SBN_INTERNAL_UNWRAP


    template <class T>
    class basic_weight_array: public std::array<T,2> {

    private:
        using base_type = std::array<T,2>;

    public:
        inline explicit basic_weight_array(T a, T b): base_type{a,b} {}
        using base_type::size;

        basic_weight_array() = default;
        ~basic_weight_array() = default;
        basic_weight_array(const basic_weight_array&) = default;
        basic_weight_array& operator=(const basic_weight_array&) = default;
        basic_weight_array(basic_weight_array&&) = default;
        basic_weight_array& operator=(basic_weight_array&&) = default;

        #define SBN_INTERNAL_OPERATOR(OP) \
            inline basic_weight_array& operator OP(const basic_weight_array& rhs) noexcept { \
                const auto n = size(); \
                for (size_t i=0; i<n; ++i) { (*this)[i] OP rhs[i]; } \
                return *this; \
            } \
            inline basic_weight_array& operator OP(T rhs) noexcept { \
                const auto n = size(); \
                for (size_t i=0; i<n; ++i) { (*this)[i] OP rhs; } \
                return *this; \
            }

        SBN_INTERNAL_OPERATOR(+=);
        SBN_INTERNAL_OPERATOR(-=);
        SBN_INTERNAL_OPERATOR(*=);
        SBN_INTERNAL_OPERATOR(/=);
        SBN_INTERNAL_OPERATOR(%=);

        #undef SBN_INTERNAL_OPERATOR

    };

    using weight_type = basic_weight<sys::u32>;
    using weight_array = basic_weight_array<weight_type>;
    using modular_weight_array = basic_weight_array<std::array<weight_type,2>>;

    std::ostream& operator<<(std::ostream& out, const weight_array& rhs);
    std::ostream& operator<<(std::ostream& out, const modular_weight_array& rhs);

    kernel_buffer& operator<<(kernel_buffer& out, const weight_array& rhs);
    kernel_buffer& operator>>(kernel_buffer& in, weight_array& rhs);

}

#endif // vim:filetype=cpp
