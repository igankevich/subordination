#include "mobile_kernel.hh"

void
asc::mobile_kernel::read(sys::pstream& in) {
	in >> _result >> _id;
}

void
asc::mobile_kernel::write(sys::pstream& out) {
	out << _result << _id;
}
