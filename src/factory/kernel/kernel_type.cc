#include "kernel_type.hh"

#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const kernel_type& rhs) {
	return out << rhs.name() << '(' << rhs.id() << ')';
}

