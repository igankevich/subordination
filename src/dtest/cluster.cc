#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <dtest/cluster.hh>

void dts::cluster::generate_nodes(size_t num_nodes) {
    std::vector<cluster_node> result;
    auto num_addresses = this->_network.count()-2;
    if (num_addresses < num_nodes) {
        throw std::invalid_argument("insufficient network size");
    }
    auto address = this->_network.begin();
    auto peer_address = this->_peer_network.begin();
    auto ndigits = num_nodes/10+1;
    for (size_t i=0; i<num_nodes; ++i) {
        result.emplace_back();
        auto& node = result.back();
        std::stringstream tmp;
        tmp << name() << std::setw(ndigits) << std::setfill('0') << (i+1);
        node.name(tmp.str());
        node.interface_address({*address++,this->_network.netmask()});
        node.peer_interface_address({*peer_address++,this->_peer_network.netmask()});
    }
    this->_nodes = std::move(result);
}

dts::cluster_node& dts::cluster::node(std::string name) {
    auto result = std::find_if(this->_nodes.begin(), this->_nodes.end(),
                               [&] (const cluster_node& node) { return node.name() == name; });
    if (result == this->_nodes.end()) {
        std::stringstream tmp;
        tmp << "node \"" << name << "\" not found";
        throw std::invalid_argument(tmp.str());
    }
    return *result;
}
