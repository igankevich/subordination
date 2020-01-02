#include "probe.hh"

void
bsc::probe::write(sys::pstream& out) const {
    bsc::kernel::write(out);
    out << this->_ifaddr << this->_oldprinc << this->_newprinc;
}

void
bsc::probe::read(sys::pstream& in) {
    bsc::kernel::read(in);
    in >> this->_ifaddr >> this->_oldprinc >> this->_newprinc;
}

