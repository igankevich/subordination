#include <subordination/daemon/hierarchy_node.hh>

std::ostream&
sbn::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    if (rhs.socket_address()) { out << rhs.socket_address() << '*' << rhs.weight(); }
    return out;
}

void sbn::hierarchy_node::write(sys::pstream& out) const {
    out << this->_endpoint;
    out << this->_weight;
}

void sbn::hierarchy_node::read(sys::pstream& in) {
    in >> this->_endpoint;
    in >> this->_weight;
}
