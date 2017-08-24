#include "mobile_kernel.hh"

void
factory::Mobile_kernel::read(sys::pstream& in) {
	in >> _result >> _id;
}

void
factory::Mobile_kernel::write(sys::pstream& out) {
	out << _result << _id;
}
