#include <ostream>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_type.hh>

sbn::kernel_ptr sbn::kernel_type::construct() const { return this->_construct(); }

std::ostream&
sbn::operator<<(std::ostream& out, const kernel_type& rhs) {
    return out << rhs.name() << '(' << rhs.id() << ')';
}
