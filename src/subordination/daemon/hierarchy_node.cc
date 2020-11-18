#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy_node.hh>

std::ostream&
sbnd::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    out << rhs.weight();
    return out;
}

void sbnd::hierarchy_node::write(sbn::kernel_buffer& out) const {
    out << this->_weight;
}

void sbnd::hierarchy_node::read(sbn::kernel_buffer& in) {
    in >> this->_weight;
}
