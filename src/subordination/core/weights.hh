#ifndef SUBORDINATION_CORE_WEIGHTS_HH
#define SUBORDINATION_CORE_WEIGHTS_HH

#include <array>
#include <cstdint>
#include <iosfwd>

#include <subordination/core/types.hh>

namespace sbn {

    using weight_type = std::uint32_t;

    class weight_array: public std::array<weight_type,2> {

    private:
        using base_type = std::array<weight_type,2>;

    public:
        inline explicit weight_array(weight_type a, weight_type b): base_type{a,b} {}
        using base_type::size;

        weight_array() = default;
        ~weight_array() = default;
        weight_array(const weight_array&) = default;
        weight_array& operator=(const weight_array&) = default;
        weight_array(weight_array&&) = default;
        weight_array& operator=(weight_array&&) = default;

        #define SBN_INTERNAL_OPERATOR(OP) \
            inline weight_array& operator OP(const weight_array& rhs) noexcept { \
                const auto n = size(); \
                for (size_t i=0; i<n; ++i) { (*this)[i] OP rhs[i]; } \
                return *this; \
            } \
            inline weight_array& operator OP(weight_type rhs) noexcept { \
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

    std::ostream& operator<<(std::ostream& out, const weight_array& rhs);

    kernel_buffer& operator<<(kernel_buffer& out, const weight_array& rhs);
    kernel_buffer& operator>>(kernel_buffer& in, weight_array& rhs);

}

#endif // vim:filetype=cpp
