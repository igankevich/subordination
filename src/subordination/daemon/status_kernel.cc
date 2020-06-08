#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/status_kernel.hh>

void sbnd::Status_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_hierarchies;
}

void sbnd::Status_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_hierarchies;
}
