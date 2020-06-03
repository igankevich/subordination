#ifndef SUBORDINATION_PPL_SOCKET_PIPELINE_HH
#define SUBORDINATION_PPL_SOCKET_PIPELINE_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/net/interface_address>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <subordination/kernel/kernel_instance_registry.hh>
#include <subordination/kernel/kstream.hh>
#include <subordination/ppl/basic_socket_pipeline.hh>
#include <subordination/ppl/local_server.hh>
#include <subordination/ppl/types.hh>

namespace sbn {

    class socket_pipeline: public basic_socket_pipeline {

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
        using id_type = mobile_kernel::id_type;
        using weight_type = uint32_t;

    private:
        server_array _servers;
        client_table _clients;
        /// Iterator to client container which is used to distribute the
        /// kernels between several clients taking into account their weight.
        client_iterator _iterator = this->_clients.end();
        /// Client weight counter. Goes from nought to the number of nodes
        /// "behind" the client.
        weight_type _weightcnt = 0;
        sys::port_type _port = 33333;
        std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);
        id_type _counter = 0;
        bool _uselocalhost = true;

    public:

        socket_pipeline();
        ~socket_pipeline() = default;
        socket_pipeline(const socket_pipeline&) = delete;
        socket_pipeline(socket_pipeline&&) = delete;
        socket_pipeline& operator=(const socket_pipeline&) = delete;
        socket_pipeline& operator=(socket_pipeline&&) = delete;

        void
        add_client(const sys::socket_address& addr) {
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

        void forward(foreign_kernel* hdr) override;

        inline void
        set_port(sys::port_type rhs) noexcept {
            this->_port = rhs;
        }

        inline sys::port_type
        port() const noexcept {
            return this->_port;
        }

        inline server_const_iterator
        servers_begin() const noexcept {
            return this->_servers.begin();
        }

        inline server_const_iterator
        servers_end() const noexcept {
            return this->_servers.end();
        }

        inline void
        use_localhost(bool b) noexcept {
            this->_uselocalhost = b;
        }

        void
        remove_server(const interface_address& interface_address);

        void
        print_state(std::ostream& out);

    private:

        void
        remove_client(const sys::socket_address& vaddr);

        void
        remove_client(client_iterator result);

        void
        remove_server(server_iterator result);

        server_iterator
        find_server(const interface_address& interface_address);

        server_iterator
        find_server(sys::fd_type fd);

        server_iterator
        find_server(const sys::socket_address& dest);

        void
        ensure_identity(kernel* k, const sys::socket_address& dest);

        /// round robin over upstream hosts
        void
        find_next_client();

        inline bool
        end_reached() const noexcept {
            return this->_iterator == this->_clients.end();
        }

        inline void
        reset_iterator() noexcept {
            this->_iterator = this->_clients.end();
            this->_weightcnt = 0;
        }

        inline const remote_client&
        current_client() const noexcept {
            return *this->_iterator->second;
        }

        inline remote_client&
        current_client() noexcept {
            return *this->_iterator->second;
        }

        inline void
        advance_client_iterator() noexcept {
            ++this->_iterator;
            this->_weightcnt = 0;
        }

        void
        emplace_client(const sys::socket_address& vaddr, const client_ptr& s);

        inline sys::socket_address
        virtual_addr(const sys::socket_address& addr) const {
            return addr.family() == sys::family_type::unix
                   ? addr
                   : sys::socket_address(addr, this->_port);
        }

        void
        process_kernels() override;

        void
        process_kernel(kernel* k);

        client_ptr
        find_or_create_client(const sys::socket_address& addr);

        client_ptr
        do_add_client(const sys::socket_address& addr);

        client_ptr
        do_add_client(sys::socket&& sock, sys::socket_address vaddr);

        void fire_event_kernels(socket_pipeline_kernel* event);

        friend class local_server;
        friend class remote_client;

    };

}

#endif // vim:filetype=cpp
