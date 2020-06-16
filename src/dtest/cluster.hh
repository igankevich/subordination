#ifndef DTEST_CLUSTER_HH
#define DTEST_CLUSTER_HH

#include <string>
#include <vector>

#include <unistdx/net/bridge_interface>

#include <dtest/cluster_node.hh>

namespace dts {

    class cluster {

    public:
        using address_type = cluster_node::address_type;

    private:
        std::string _name{"a"};
        address_type _network{{10,1,0,1},16};
        address_type _peer_network{{10,0,0,1},16};
        std::vector<cluster_node> _nodes;
        sys::bridge_interface _bridge;

    public:
        inline const std::string& name() const { return this->_name; }
        inline void name(const std::string& name) { this->_name = name; }
        inline size_t size() const { return this->_nodes.size(); }
        inline void network(address_type rhs) { this->_network = rhs; }
        inline void peer_network(address_type rhs) { this->_peer_network = rhs; }
        inline const sys::bridge_interface& bridge() const noexcept { return this->_bridge; }
        inline void bridge(sys::bridge_interface&& rhs) { this->_bridge = std::move(rhs); }
        inline const std::vector<cluster_node>& nodes() const noexcept { return this->_nodes; }
        inline std::vector<cluster_node>& nodes() noexcept { return this->_nodes; }
        void generate_nodes(size_t n);
        cluster_node& node(std::string name);

    };

}

#endif // vim:filetype=cpp
