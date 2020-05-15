#ifndef DTEST_CLUSTER_HH
#define DTEST_CLUSTER_HH

#include <string>
#include <vector>

#include <dtest/cluster_node.hh>

namespace dts {

    class cluster {

    public:
        using address_type = cluster_node::address_type;

    private:
        std::string _name{"a"};
        address_type _network{{10,1,0,1},16};
        address_type _peer_network{{10,0,0,1},16};
        size_t _size = 1;
        std::vector<cluster_node> _nodes;

    public:
        inline const std::string& name() const { return this->_name; }
        inline void name(const std::string& name) { this->_name = name; }
        inline void size(size_t n) { this->_size = n; }
        inline size_t size() const { return this->_size; }
        inline void network(address_type rhs) { this->_network = rhs; }
        inline void peer_network(address_type rhs) { this->_peer_network = rhs; }
        auto nodes() -> std::vector<cluster_node>;

    };

}

#endif // vim:filetype=cpp
