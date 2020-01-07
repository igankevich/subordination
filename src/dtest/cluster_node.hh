#ifndef DTEST_CLUSTER_NODE_HH
#define DTEST_CLUSTER_NODE_HH

#include <string>

#include <unistdx/net/ipv4_address>
#include <unistdx/net/interface_address>

namespace dts {

    class cluster_node {

    public:
        using address_type = sys::interface_address<sys::ipv4_address>;

    private:
        std::string _name;
        address_type _address;
        address_type _peer;

    public:
        inline const std::string& name() const { return this->_name; }
        inline const address_type& interface_address() const { return this->_address; }
        inline const address_type& peer_interface_address() const { return this->_peer; }
        inline void name(const std::string& name) { this->_name = name; }
        inline void interface_address(const address_type& rhs) { this->_address = rhs; }
        inline void peer_interface_address(const address_type& rhs) { this->_peer = rhs; }

    };

}

#endif // vim:filetype=cpp
