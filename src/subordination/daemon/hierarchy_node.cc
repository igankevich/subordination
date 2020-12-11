#include <subordination/core/kernel_buffer.hh>
#include <subordination/daemon/hierarchy_node.hh>

std::ostream&
sbnd::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    using r = sbn::resources::resources;
    return out << rhs.resources()[r::num_threads];
}

void sbnd::hierarchy_node::write(sbn::kernel_buffer& out) const {
    this->_resources.write(out);
}

void sbnd::hierarchy_node::read(sbn::kernel_buffer& in) {
    this->_resources.read(in);
}
