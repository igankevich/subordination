#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <dtest/cluster_node_bitmap.hh>

dts::cluster_node_bitmap::cluster_node_bitmap(size_t n, index_array indices):
_nodes(n, false) {
    for (auto i : indices) {
        if (i >= n) {
            std::stringstream tmp;
            tmp << "invalid node index \"" << i << "\"";
            throw std::invalid_argument(tmp.str());
        }
        this->_nodes[i] = true;
    }
}

void dts::cluster_node_bitmap::read(std::string arg) {
    if (arg == "*") {
        std::fill(this->_nodes.begin(), this->_nodes.end(), true);
    } else {
        arg += ',';
        std::string tmp;
        for (auto ch : arg) {
            if (ch == ',') {
                std::stringstream s(tmp);
                size_t n = 0;
                if (!(s >> n) || n == 0 || n > this->_nodes.size()) {
                    std::stringstream msg;
                    msg << "invalid node number: " << n;
                    throw std::invalid_argument(msg.str());
                }
                --n;
                this->_nodes[n] = true;
                tmp.clear();
            } else { tmp += ch; }
        }
    }
}

bool dts::cluster_node_bitmap::intersects(const cluster_node_bitmap& rhs) const {
    const auto n = size();
    if (n != rhs.size()) { throw std::invalid_argument("bad bitmap size"); }
    for (size_t i=0; i<n; ++i) {
        if (this->_nodes[i] && rhs._nodes[i]) { return true; }
    }
    return false;
}

std::ostream& dts::operator<<(std::ostream& out, const cluster_node_bitmap& rhs) {
    const auto n = rhs.size();
    bool first = true;
    for (size_t i=0; i<n; ++i) {
        if (rhs._nodes[i]) {
            if (!first) { out << ','; }
            out << i+1;
            if (first) { first = false; }
        }
    }
    return out;
}
