#include "error.hh"
#include <ostream>

std::ostream&
bsc::operator<<(std::ostream& out, const error& rhs) {
	return out << rhs._file << ':' << rhs._line << ':' << rhs._func
		<< ' ' << rhs.what();
}

