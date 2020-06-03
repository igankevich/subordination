#include <subordination/daemon/probe.hh>

void
sbnd::probe::write(sys::pstream& out) const {
    sbn::kernel::write(out);
    out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
sbnd::probe::read(sys::pstream& in) {
    sbn::kernel::read(in);
    in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}
