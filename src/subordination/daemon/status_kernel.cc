#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/status_kernel.hh>

void sbn::Status_kernel::write(sys::pstream& out) const {
    kernel::write(out);
    out << this->_hierarchies;
}

void sbn::Status_kernel::read(sys::pstream& in) {
    kernel::read(in);
    in >> this->_hierarchies;
}
