#include "probe.hh"

void
factory::probe::write(sys::pstream& out) {
	factory::api::Kernel::write(out);
	out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
factory::probe::read(sys::pstream& in) {
	factory::api::Kernel::read(in);
	in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}

