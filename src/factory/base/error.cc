#include "error.hh"
#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const Error& rhs) {
	return out << rhs._file << ':' << rhs._line << ':' << rhs._func
		<< ' ' << rhs.what();
}

