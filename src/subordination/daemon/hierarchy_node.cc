#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy_node.hh>

std::ostream&
sbnd::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    using r = sbn::resources::resources;
    return out << rhs.resources()[r::total_threads];
}

void sbnd::hierarchy_node::write(sbn::kernel_buffer& out) const {
    out << this->_socket_address;
    out << this->_superior_socket_address;
    this->_resources.write(out);
    out << this->_version;
}

void sbnd::hierarchy_node::read(sbn::kernel_buffer& in) {
    in >> this->_socket_address;
    in >> this->_superior_socket_address;
    this->_resources.read(in);
    in >> this->_version;
}
