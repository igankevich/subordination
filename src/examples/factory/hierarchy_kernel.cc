#include "hierarchy_kernel.hh"

void
asc::hierarchy_kernel::write(sys::pstream& out) {
	asc::kernel::write(out);
	out << this->_ifaddr << this->_weight;
}

void
asc::hierarchy_kernel::read(sys::pstream& in) {
	asc::kernel::read(in);
	in >> this->_ifaddr >> this->_weight;
}

