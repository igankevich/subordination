#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <dtest/cluster.hh>

auto dts::cluster::nodes() -> std::vector<cluster_node> {
    std::vector<cluster_node> result;
    auto num_nodes = this->_size;
    auto num_addresses = this->_network.count()-2;
    if (num_addresses < num_nodes) {
        throw std::invalid_argument("insufficient network size");
    }
    auto address = this->_network.begin();
    auto peer_address = this->_peer_network.begin();
    auto ndigits = std::to_string(num_nodes).size();
    for (size_t i=0; i<num_nodes; ++i) {
        result.emplace_back();
        auto& node = result.back();
        std::stringstream tmp;
        tmp << name() << std::setw(ndigits) << std::setfill('0') << (i+1);
        node.name(tmp.str());
        node.interface_address({*address++,this->_network.netmask()});
        node.peer_interface_address({*peer_address++,this->_peer_network.netmask()});
    }
    return result;
}
