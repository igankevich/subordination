#include "probe.hh"

void
asc::probe::write(sys::pstream& out) {
	asc::kernel::write(out);
	out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
asc::probe::read(sys::pstream& in) {
	asc::kernel::read(in);
	in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}

