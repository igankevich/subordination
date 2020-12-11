#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy_kernel.hh>

void
sbnd::Hierarchy_kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_interface_address;
    this->_resources.write(out);
}

void
sbnd::Hierarchy_kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_interface_address;
    this->_resources.read(in);
}
