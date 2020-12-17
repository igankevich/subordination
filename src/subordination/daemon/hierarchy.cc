#include <algorithm>

#include <unistdx/it/intersperse_iterator>
#include <unistdx/net/ipv4_address>

#include <subordination/daemon/byte_buffers.hh>
#include <subordination/daemon/hierarchy.hh>

template <class T> bool
sbnd::Hierarchy<T>::add_nodes(const hierarchy_node_array& nodes, time_point now) {
    bool updated = false;
    const auto& n = this->_this_node;
    for (const auto& a : nodes) {
        if (a.socket_address() == n.socket_address()) { continue; }
        auto result = this->_nodes.find(a.socket_address());
        if (result == this->_nodes.end()) {
            auto r = this->_nodes.emplace(a.socket_address(), a);
            if (r.second) { r.first->second.last_modified(now); }
            updated = true;
        } else {
            auto& b = result->second;
            if (b.version() < a.version()) {
                b = a;
                updated = true;
            }
            if (b.version() <= a.version()) {
                b.last_modified(now);
            }
        }
    }
    return updated;
}

template <class T> bool
sbnd::Hierarchy<T>::add_superior(const hierarchy_node& node, time_point now) {
    bool updated = false;
    auto& n = this->_this_node;
    if (n.superior_socket_address() != node.socket_address()) {
        n.superior_socket_address(node.socket_address());
        n.increment_version();
        n.last_modified(now);
        updated = true;
    }
    return updated;
}

template <class T> bool
sbnd::Hierarchy<T>::add_subordinate(const hierarchy_node& node, time_point now) {
    const auto& n = this->_this_node;
    auto& b = this->_nodes[node.socket_address()];
    bool updated = false;
    if (b.superior_socket_address() != n.socket_address()) {
        b.superior_socket_address(n.socket_address());
        b.last_modified(now);
        updated = true;
    }
    return updated;
}

template <class T> bool
sbnd::Hierarchy<T>::remove_node(const sys::socket_address& sa, time_point now) {
    auto& n = this->_this_node;
    bool updated = false;
    if (n.superior_socket_address() == sa) {
        n.superior_socket_address({});
        n.last_modified(now);
        updated = true;
    }
    auto result = this->_nodes.find(sa);
    if (result == this->_nodes.end()) { return false; }
    auto& b = result->second;
    if (b.superior_socket_address() == n.socket_address()) {
        b.superior_socket_address({});
        b.last_modified(now);
        updated = true;
    } else {
        b.last_modified(now);
    }
    return updated;
}

template <class T> bool
sbnd::Hierarchy<T>::resources(const resource_array& rhs, time_point now) {
    auto& n = this->_this_node;
    if (n.resources() != rhs) {
        n.resources(rhs);
        n.increment_version();
        n.last_modified(now);
        return true;
    }
    return false;
}

template <class T> std::ostream&
sbnd::operator<<(std::ostream& out, const Hierarchy<T>& rhs) {
    auto result = rhs._nodes.find(rhs._this_node.superior_socket_address());
    out << "interface-address=" << rhs.interface_address() << ',';
    out << "superior=";
    if (result != rhs._nodes.end()) { out << result->second; }
    out << ',';
    out << "subordinates=";
    bool first = true;
    for (const auto& pair : rhs._nodes) {
        if (pair.first == rhs._this_node.superior_socket_address()) { continue; }
        if (pair.second.superior_socket_address() != rhs._this_node.socket_address()) {
            continue;
        }
        out << pair.first << '*' << pair.second.total_threads();
        if (!first) { out << ','; }
        first = false;
    }
    return out;
}

template <class T>
auto sbnd::Hierarchy<T>::nodes(int radius) const -> hierarchy_node_array {
    hierarchy_node_array result;
    result.reserve(this->_nodes.size()+1);
    result.emplace_back(this->_this_node);
    // add lower nodes
    traverse(result, 0, radius, [] (const hierarchy_node& b, const hierarchy_node& a) {
        return b.superior_socket_address() == a.socket_address();
    });
    const int old_size = result.size();
    result.emplace_back(this->_this_node);
    // add upper nodes
    traverse(result, old_size, radius, [&result] (const hierarchy_node& b, const hierarchy_node& a) {
        return a.superior_socket_address() == b.socket_address() &&
            std::find_if(result.begin(), result.end(),
                         [&b] (const hierarchy_node& x) {
                             return x.socket_address() == b.socket_address();
                         }) == result.end();
    });
    result.erase(result.begin() + old_size);
    return result;
}

template <class T> auto
sbnd::Hierarchy<T>::lower_nodes(const hierarchy_node& from, int depth) const
-> hierarchy_node_array {
    hierarchy_node_array result;
    result.reserve(this->_nodes.size()+1);
    result.emplace_back(from);
    traverse(result, 0, depth, [] (const hierarchy_node& b, const hierarchy_node& a) {
        return b.superior_socket_address() == a.socket_address();
    });
    result.shrink_to_fit();
    return result;
}

template <class T> auto
sbnd::Hierarchy<T>::upper_nodes(const hierarchy_node& from, int depth) const
-> hierarchy_node_array {
    hierarchy_node_array result;
    result.reserve(this->_nodes.size()+1);
    result.emplace_back(from);
    traverse(result, 0, depth, [&result] (const hierarchy_node& b, const hierarchy_node& a) {
        return a.superior_socket_address() == b.socket_address() &&
            std::find_if(result.begin(), result.end(),
                         [&b] (const hierarchy_node& x) {
                             return x.socket_address() == b.socket_address();
                         }) == result.end();
    });
    result.shrink_to_fit();
    return result;
}

template <class T> void
sbnd::Hierarchy<T>::write(sbn::kernel_buffer& out) const {
    out << this->_netmask;
    out << this->_this_node;
    out << this->_nodes;
}

template <class T> void
sbnd::Hierarchy<T>::read(sbn::kernel_buffer& in) {
    in >> this->_netmask;
    in >> this->_this_node;
    in >> this->_nodes;
}

template class sbnd::Hierarchy<sys::ipv4_address>;

template std::ostream&
sbnd::operator<<(std::ostream& out, const Hierarchy<sys::ipv4_address>& rhs);
