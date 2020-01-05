#include "hierarchy.hh"

#include <algorithm>

#include <unistdx/net/ipv4_address>
#include <unistdx/it/intersperse_iterator>

template <class Addr>
std::ostream&
sbn::operator<<(std::ostream& out, const hierarchy<Addr>& rhs) {
    out << "interface_address=" << rhs._ifaddr << ',';
    out << "principal=" << rhs._principal << ',';
    out << "subordinates=";
    std::copy(
        rhs._subordinates.begin(),
        rhs._subordinates.end(),
        sys::intersperse_iterator<hierarchy_node,char>(out, ',')
    );
    return out;
}

template <class Addr>
bool
sbn::hierarchy<Addr>::set_subordinate_weight(
    const sys::socket_address& endp,
    weight_type w
) {
    bool changed = false;
    auto result = this->_subordinates.find(node_type(endp));
    if (result != this->_subordinates.end()) {
        weight_type old = result->weight();
        changed = old != w;
        if (changed) {
            result->weight(w);
        }
    }
    return changed;
}

template <class Addr>
typename sbn::hierarchy<Addr>::weight_type
sbn::hierarchy<Addr>::total_weight() const noexcept {
    // add 1 for the current node
    return this->principal_weight() + this->total_subordinate_weight() + 1;
}

template <class Addr>
typename sbn::hierarchy<Addr>::weight_type
sbn::hierarchy<Addr>::total_subordinate_weight() const noexcept {
    weight_type sum = 0;
    for (const node_type& n : this->_subordinates) {
        sum += n.weight();
    }
    return sum;
}

template class sbn::hierarchy<sys::ipv4_address>;

template std::ostream&
sbn::operator<<(std::ostream& out, const hierarchy<sys::ipv4_address>& rhs);
