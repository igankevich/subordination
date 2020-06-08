#include <subordination/core/kernel_type.hh>

#include <ostream>

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_type& rhs) {
    return out << rhs.name() << '(' << rhs.id() << ')';
}
