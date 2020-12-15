#include <algorithm>

#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/ipv4_address>

#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/hierarchy.hh>

template <class T> bool
sbnd::Hierarchy<T>::add_subordinate(const sys::socket_address& addr) {
    auto result = this->_subordinates.find(addr);
    if (result != this->_subordinates.end()) { return false; }
    this->_subordinates.emplace(addr, hierarchy_node{});
    return true;
}

template <class T>
std::ostream&
sbnd::operator<<(std::ostream& out, const Hierarchy<T>& rhs) {
    out << "interface-address=" << rhs._ifaddr << ',';
    out << "principal=" << rhs._superior << ',';
    out << "subordinates=";
    bool first = true;
    for (const auto& pair : rhs._subordinates) {
        out << pair.first << '*' << pair.second.total_threads();
        if (!first) { out << ','; }
        first = false;
    }
    return out;
}

template <class T>
bool
sbnd::Hierarchy<T>::set_subordinate(
    const sys::socket_address& address,
    const hierarchy_node& new_sub
) {
    auto result = this->_subordinates.find(address);
    if (result == this->_subordinates.end()) { return false; }
    if (new_sub != result->second) {
        result->second = new_sub;
        return true;
    }
    return false;
}

template <class T> auto
sbnd::Hierarchy<T>::total_resources() const noexcept -> resource_array {
    resource_array sum;
    sum += superior_resources();
    /// total resources of all subordinate nodes
    for (const auto& pair : this->_subordinates) { sum += pair.second.resources(); }
    sum += this->_resources;
    return sum;
}

template <class T> void
sbnd::Hierarchy<T>::write(sbn::kernel_buffer& out) const {
    out << this->_ifaddr;
    out << this->_socket_address;
    out << this->_superior_socket_address;
    out << this->_superior;
    out << this->_subordinates;
}

template <class T> void
sbnd::Hierarchy<T>::read(sbn::kernel_buffer& in) {
    in >> this->_ifaddr;
    in >> this->_socket_address;
    in >> this->_superior_socket_address;
    in >> this->_superior;
    in >> this->_subordinates;
}

template class sbnd::Hierarchy<sys::ipv4_address>;

template std::ostream&
sbnd::operator<<(std::ostream& out, const Hierarchy<sys::ipv4_address>& rhs);
