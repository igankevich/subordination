#include "mobile_kernel.hh"

void
sbn::mobile_kernel::read(sys::pstream& in) {
    in >> _result >> _id;
}

void
sbn::mobile_kernel::write(sys::pstream& out) const {
    out << _result << _id;
}
