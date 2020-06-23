#ifndef DTEST_CLUSTER_NODE_HH
#define DTEST_CLUSTER_NODE_HH

#include <string>

#include <unistdx/io/fildes>
#include <unistdx/ipc/process>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>
#include <unistdx/net/veth_interface>

namespace dts {

    class cluster_node {

    public:
        using address_type = sys::interface_address<sys::ipv4_address>;

    private:
        std::string _name;
        address_type _address;
        address_type _peer;
        sys::veth_interface _veth;
        sys::fildes _network_namespace;
        sys::fildes _hostname_namespace;

    public:
        inline const std::string& name() const { return this->_name; }
        inline const address_type& interface_address() const { return this->_address; }
        inline const address_type& peer_interface_address() const { return this->_peer; }
        inline void name(const std::string& name) { this->_name = name; }
        inline void interface_address(const address_type& rhs) { this->_address = rhs; }
        inline void peer_interface_address(const address_type& rhs) { this->_peer = rhs; }
        inline const sys::veth_interface& veth() const noexcept { return this->_veth; }
        inline sys::veth_interface& veth() noexcept { return this->_veth; }
        inline void veth(sys::veth_interface&& rhs) { this->_veth = std::move(rhs); }

        inline const sys::fildes& network_namespace() const {
            return this->_network_namespace;
        }

        inline void network_namespace(sys::fildes&& rhs) {
            this->_network_namespace = std::move(rhs);
        }

        inline const sys::fildes& hostname_namespace() const {
            return this->_hostname_namespace;
        }

        inline void hostname_namespace(sys::fildes&& rhs) {
            this->_hostname_namespace = std::move(rhs);
        }

        template <class Function>
        void run(Function func) {
            const auto old_network_namespace = sys::this_process::get_namespace("net");
            const auto old_hostname_namespace = sys::this_process::get_namespace("uts");
            sys::this_process::enter(hostname_namespace().fd());
            sys::this_process::enter(network_namespace().fd());
            func();
            sys::this_process::enter(old_network_namespace.fd());
            sys::this_process::enter(old_hostname_namespace.fd());
        }

    };

}

#endif // vim:filetype=cpp
