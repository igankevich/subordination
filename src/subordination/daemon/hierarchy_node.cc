#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy_node.hh>

std::ostream&
sbnd::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    if (rhs.socket_address()) { out << rhs.socket_address() << '*' << rhs.weight(); }
    return out;
}

void sbnd::hierarchy_node::write(sbn::kernel_buffer& out) const {
    out << this->_endpoint;
    out << this->_weight;
}

void sbnd::hierarchy_node::read(sbn::kernel_buffer& in) {
    in >> this->_endpoint;
    in >> this->_weight;
}
