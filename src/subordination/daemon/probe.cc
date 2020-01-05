#include <subordination/daemon/probe.hh>

void
sbn::probe::write(sys::pstream& out) const {
    sbn::kernel::write(out);
    out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
sbn::probe::read(sys::pstream& in) {
    sbn::kernel::read(in);
    in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}
