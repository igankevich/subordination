#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <dtest/cluster_node_bitmap.hh>

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
