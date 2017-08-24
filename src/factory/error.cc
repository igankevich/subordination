#include "error.hh"
#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const Error_location& rhs) {
	return out << rhs._file << ':' << rhs._line << ':' << rhs._func;
}

std::ostream&
factory::operator<<(std::ostream& out, const Error& rhs) {
	return out << rhs._location << ' ' << rhs.what();
}

