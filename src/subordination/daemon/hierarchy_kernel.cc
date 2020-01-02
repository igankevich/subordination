#include "hierarchy_kernel.hh"

void
bsc::hierarchy_kernel::write(sys::pstream& out) const {
    bsc::kernel::write(out);
    out << this->_ifaddr << this->_weight;
}

void
bsc::hierarchy_kernel::read(sys::pstream& in) {
    bsc::kernel::read(in);
    in >> this->_ifaddr >> this->_weight;
}

