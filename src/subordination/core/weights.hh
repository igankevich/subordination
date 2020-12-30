#ifndef SUBORDINATION_CORE_WEIGHTS_HH
#define SUBORDINATION_CORE_WEIGHTS_HH

#include <array>
#include <cstdint>
#include <ostream>

#include <subordination/core/types.hh>

namespace sbn {

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

    using weight_array = basic_weight_array<weight_type>;
    using modular_weight_array = basic_weight_array<std::array<weight_type,2>>;

    std::ostream& operator<<(std::ostream& out, const weight_array& rhs);
    std::ostream& operator<<(std::ostream& out, const modular_weight_array& rhs);

    kernel_buffer& operator<<(kernel_buffer& out, const weight_array& rhs);
    kernel_buffer& operator>>(kernel_buffer& in, weight_array& rhs);

}

#endif // vim:filetype=cpp
