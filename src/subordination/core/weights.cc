#include <ostream>

#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/weights.hh>

std::ostream& sbn::operator<<(std::ostream& out, const weight_array& rhs) {
    out << '(';
    constexpr const auto n = rhs.size();
    if (n != 0) { out << rhs[0]; }
    for (size_t i=1; i<n; ++i) { out << ' ' << rhs[i]; }
    out << ')';
    return out;
}

std::ostream& sbn::operator<<(std::ostream& out, const modular_weight_array& rhs) {
    out << '(';
    constexpr const auto n = rhs.size();
    if (n != 0) { out << '(' << rhs[0][0] << ' ' << rhs[0][1] << ')'; }
    for (size_t i=1; i<n; ++i) { out << " (" << rhs[i][0] << ' ' << rhs[i][1] << ')'; }
    out << ')';
    return out;
}

sbn::kernel_buffer& sbn::operator<<(kernel_buffer& out, const weight_array& rhs) {
    for (auto w : rhs) { out.write(w.get()); }
    return out;
}

sbn::kernel_buffer& sbn::operator>>(kernel_buffer& in, weight_array& rhs) {
    for (auto& w : rhs) { in.read(w.get()); }
    return in;
}
