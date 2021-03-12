#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/probe.hh>

void
sbnd::probe::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    out << this->_ifaddr;
    out << this->_old_superior;
    out << this->_new_superior;
    out << this->_nodes;
}

void
sbnd::probe::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    in >> this->_ifaddr;
    in >> this->_old_superior;
    in >> this->_new_superior;
    in >> this->_nodes;
}
