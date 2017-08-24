#include "kernel_error.hh"

#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const Kernel_error& rhs) {
	return out << rhs.what() << ": id=" << rhs.id();
}

