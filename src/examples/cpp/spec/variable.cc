#include <stdexcept>

#include <examples/spec/variable.hh>

auto spec::string_to_variable(const std::string& s) -> Variable {
    if (s.empty()) { throw std::invalid_argument("bad variable"); }
    switch (s.front()) {
        case 'd': return Variable::D;
        case 'i': return Variable::I;
        case 'j': return Variable::J;
        case 'k': return Variable::K;
        case 'w': return Variable::W;
        default: throw std::invalid_argument("bad variable");
    }
}
