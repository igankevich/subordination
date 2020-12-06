#ifndef EXAMPLES_CPP_SPEC_VARIABLE_HH
#define EXAMPLES_CPP_SPEC_VARIABLE_HH

#include <string>
#include <type_traits>

namespace spec {

    constexpr const auto max_variables = 5;

    enum class Variable: int {D=0, I=1, J=2, K=3, W=4};

    auto string_to_variable(const std::string& s) -> Variable;

}

namespace std {
    template <>
    class hash<spec::Variable>: public hash<std::underlying_type<spec::Variable>::type> {
    public:
        size_t operator()(spec::Variable v) const noexcept {
            using tp = std::underlying_type<spec::Variable>::type;
            return this->hash<tp>::operator()(tp(v));
        }
    };
}

#endif // vim:filetype=cpp
