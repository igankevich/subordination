#ifndef SUBORDINATION_DAEMON_SOCKET_PIPELINE_HH
#define SUBORDINATION_DAEMON_SOCKET_PIPELINE_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/net/interface_address>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/types.hh>
#include <subordination/daemon/local_server.hh>
#include <subordination/daemon/socket_pipeline_event.hh>
#include <subordination/daemon/types.hh>

namespace sbnd {

    class socket_pipeline: public sbn::basic_socket_pipeline {

    public:
        using ip_address = sys::ipv4_address;
        using interface_address = sys::interface_address<ip_address>;

    private:
        using server_ptr = std::shared_ptr<local_server>;
        using server_array = std::vector<server_ptr>;
        using server_iterator = typename server_array::iterator;
        using server_const_iterator = typename server_array::const_iterator;
        using client_ptr = std::shared_ptr<remote_client>;
        using client_table = std::unordered_map<sys::socket_address,client_ptr>;
        using client_iterator = typename client_table::iterator;
        using id_type = sbn::kernel::id_type;

    private:
        class neighbour {
        public:
            using weight_type = uint32_t;
        private:
            sys::socket_address _socket_address;
            weight_type _weight = 1;
        public:
            inline explicit neighbour(const sys::socket_address& a, weight_type w=1):
            _socket_address(a), _weight(w) {}
            neighbour() = default;
            ~neighbour() = default;
            neighbour(const neighbour&) = default;
            neighbour& operator=(const neighbour&) = default;
            neighbour(neighbour&&) = default;
            neighbour& operator=(neighbour&&) = default;
            /// The number of nodes "behind" this one in the hierarchy.
            inline weight_type weight() const noexcept { return this->_weight; }
            inline void weight(weight_type rhs) noexcept { this->_weight = rhs; }
            inline const sys::socket_address& socket_address() const noexcept {
                return this->_socket_address;
            }
            inline bool local() const noexcept { return !socket_address(); }
        };
        using neighbour_array = std::vector<neighbour>;
        using neighbour_iterator = neighbour_array::iterator;
        using weight_type = neighbour::weight_type;

    private:
        server_array _servers;
        client_table _clients;
        neighbour_array _neighbours{1};
        neighbour_iterator _neighbour_iterator = this->_neighbours.begin();
        /// Client weight counter. Goes from nought to the number of nodes
        /// "behind" the client.
        weight_type _weightcnt = 0;
        sys::port_type _port = 33333;
        std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);
        id_type _counter = 0;
        bool _uselocalhost = true;

    public:

        socket_pipeline() = default;
        ~socket_pipeline() = default;
        socket_pipeline(const socket_pipeline&) = delete;
        socket_pipeline(socket_pipeline&&) = delete;
        socket_pipeline& operator=(const socket_pipeline&) = delete;
        socket_pipeline& operator=(socket_pipeline&&) = delete;

        void add_client(const sys::socket_address& addr) {
            lock_type lock(this->_mutex);
            this->do_add_client(addr);
        }

        void
        stop_client(const sys::socket_address& addr);

        void
        set_client_weight(const sys::socket_address& addr, weight_type new_weight);

        void
        add_server(const interface_address& rhs) {
            this->add_server(
                sys::socket_address(rhs.address(), this->_port),
                rhs.netmask()
            );
        }

        void
        add_server(const sys::socket_address& rhs, ip_address netmask);

        void forward(sbn::foreign_kernel_ptr&& hdr) override;

        inline void port(sys::port_type rhs) noexcept { this->_port = rhs; }
        inline sys::port_type port() const noexcept { return this->_port; }
        inline const server_array& servers() const noexcept { return this->_servers; }
        inline const client_table& clients() const noexcept { return this->_clients; }
        void use_localhost(bool rhs);
        inline bool use_localhost() const noexcept { return this->_uselocalhost; }
        void remove_server(const interface_address& interface_address);

    private:

        void remove_client(const sys::socket_address& vaddr);
        void remove_client(client_iterator result);
        void remove_server(server_iterator result);
        void add_neighbour(const sys::socket_address& vaddr);
        void remove_neighbour(const sys::socket_address& vaddr);
        void reset_neighbour_iterator();
        void advance_neighbour_iterator();

        server_iterator
        find_server(const interface_address& interface_address);

        server_iterator
        find_server(sys::fd_type fd);

        server_iterator
        find_server(const sys::socket_address& dest);

        void ensure_identity(sbn::kernel* k, const sys::socket_address& dest);

        void
        emplace_client(const sys::socket_address& vaddr, const client_ptr& s);

        inline sys::socket_address
        virtual_addr(const sys::socket_address& addr) const {
            return addr.family() == sys::family_type::unix
                   ? addr
                   : sys::socket_address(addr, this->_port);
        }

        void process_kernels() override;
        void process_kernel(sbn::kernel_ptr& k);

        client_ptr
        find_or_create_client(const sys::socket_address& addr);

        client_ptr
        do_add_client(const sys::socket_address& addr);

        client_ptr
        do_add_client(sys::socket&& sock, sys::socket_address vaddr);

        template <class ... Args>
        inline void fire_event_kernels(Args&& ... args) {
            if (!native_pipeline()) { return; }
            for (auto* target : this->_listeners) {
                sbn::kernel_ptr k(new socket_pipeline_kernel(std::forward<Args>(args)...));
                k->parent(k.get());
                k->principal(target);
                k->phase(sbn::kernel::phases::point_to_point);
                native_pipeline()->send(std::move(k));
            }
        }

        friend class local_server;
        friend class remote_client;

    };

}

#endif // vim:filetype=cpp
