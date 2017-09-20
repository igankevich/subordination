#include "probe.hh"

void
asc::probe::write(sys::pstream& out) {
	asc::Kernel::write(out);
	out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
asc::probe::read(sys::pstream& in) {
	asc::Kernel::read(in);
	in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}

