#include "type.hh"

#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const Type& rhs) {
	return out << rhs.name() << '(' << rhs.id() << ')';
}

