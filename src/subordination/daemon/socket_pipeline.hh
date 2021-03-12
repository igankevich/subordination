#ifndef SUBORDINATION_DAEMON_SOCKET_PIPELINE_HH
#define SUBORDINATION_DAEMON_SOCKET_PIPELINE_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/net/interface_address>
#include <unistdx/net/interface_socket_address>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/properties.hh>
#include <subordination/core/types.hh>
#include <subordination/core/weights.hh>
#include <subordination/daemon/file_system.hh>
#include <subordination/daemon/hierarchy.hh>
#include <subordination/daemon/local_server.hh>
#include <subordination/daemon/socket_pipeline_event.hh>
#include <subordination/daemon/types.hh>

namespace sbnd {

    class socket_pipeline_server: public sbn::connection {

    public:
        using ip_address = sys::ipv4_address;
        using interface_address_type = sys::interface_address<ip_address>;
        using interface_socket_address_type = sys::interface_socket_address<ip_address>;
        using id_type = typename interface_address_type::rep_type;
        using counter_type = id_type;

    private:
        interface_address_type _ifaddr;
        id_type _pos0 = 0;
        id_type _pos1 = 0;
        //counter_type _counter {0};
        sys::socket _socket;

    public:

        explicit
        socket_pipeline_server(const interface_address_type& ifaddr, sys::port_type port);

        socket_pipeline_server() = delete;
        socket_pipeline_server(const socket_pipeline_server&) = delete;
        socket_pipeline_server& operator=(const socket_pipeline_server&) = delete;
        socket_pipeline_server& operator=(socket_pipeline_server&&) = default;

        inline const interface_address_type
        interface_address() const noexcept {
            return this->_ifaddr;
        }

        inline interface_socket_address_type interface_socket_address() const {
            return {this->_ifaddr.address(), this->_ifaddr.prefix(),
                sys::socket_address_cast<sys::ipv4_socket_address>(socket_address()).port()};
        }

        inline sys::fd_type fd() const noexcept { return this->_socket.fd(); }
        inline const sys::socket& socket() const noexcept { return this->_socket; }
        inline sys::socket& socket() noexcept { return this->_socket; }

        /*
        inline id_type generate_id() noexcept {
            id_type id;
            if (this->_counter == this->_pos1) {
                id = this->_pos0;
                this->_counter = id+1;
            } else {
                id = this->_counter++;
            }
            return id;
        }
        */

        void handle(const sys::epoll_event& ev) override;
        void add(const connection_ptr& self) override;
        void remove(const connection_ptr& self) override;

        using connection::parent;
        inline socket_pipeline* parent() const noexcept {
            return reinterpret_cast<socket_pipeline*>(this->connection::parent());
        }

    };

    class socket_pipeline_scheduler {

    public:
        using client_ptr = std::shared_ptr<socket_pipeline_client>;
        using client_table = std::unordered_map<sys::socket_address,client_ptr>;
        using client_iterator = typename client_table::const_iterator;
        using server_ptr = std::shared_ptr<socket_pipeline_server>;
        using server_array = std::vector<server_ptr>;
        using counter_type = uint32_t;
        using counter_array = std::array<counter_type,2>;
        using file_system_ptr = std::shared_ptr<file_system>;
        using resource_array = sbn::resources::Bindings;

    private:
        std::vector<file_system_ptr> _file_systems;
        std::vector<sys::socket_address> _nodes;
        sbn::weight_array _local_load{};
        resource_array _local_resources;
        bool _local = true;

    public:

        client_iterator schedule(sbn::kernel* k,
                                 const client_table& clients,
                                 const server_array& servers);

        inline void local(bool rhs) noexcept { this->_local = rhs; }
        inline bool local() const noexcept { return this->_local; }

        inline const resource_array& local_resources() const noexcept {
            return this->_local_resources;
        }

        inline void local_resources(const resource_array& rhs) noexcept {
            this->_local_resources = rhs;
        }

        inline void add_file_system(file_system_ptr ptr) {
            this->_file_systems.emplace_back(std::move(ptr));
        }

        void rebase_counters(const client_table& clients);

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("scheduler", args ...);
        }

    private:

        inline const sbn::weight_array& local_load() const noexcept {
            return this->_local_load;
        }

        inline counter_type local_num_threads_behind() const noexcept {
            using r = sbn::resources::resources;
            return this->_local_resources[r::total_threads].unsigned_integer();
        }

        inline sbn::modular_weight_array local_relative_load() const noexcept {
            sbn::weight_array tmp{local_load()};
            auto nthreads = local_num_threads_behind();
            if (nthreads == 0) { nthreads = 1; }
            return sbn::modular_weight_array{{tmp[0],0},{tmp[1]/nthreads,tmp[1]%nthreads}};
        }

    };

    class socket_pipeline: public sbn::basic_socket_pipeline {

    public:
        struct properties: public sbn::basic_socket_pipeline::properties {
            sys::u32 max_connection_attempts = 1;
            sbn::Duration connection_timeout{std::chrono::seconds(7)};
            bool route = false;

            inline properties():
            properties{sys::this_process::cpu_affinity(), sys::page_size()} {}

            inline explicit
            properties(const sys::cpu_set& cpus, size_t page_size, size_t multiple=52):
            sbn::basic_socket_pipeline::properties{cpus, page_size, multiple} {}

            bool set(const char* key, const std::string& value);
        };

    public:
        using ip_address = sys::ipv4_address;
        using interface_address = sys::interface_address<ip_address>;
        using counter_type = uint32_t;

    private:
        using server_ptr = std::shared_ptr<socket_pipeline_server>;
        using server_array = std::vector<server_ptr>;
        using server_iterator = typename server_array::iterator;
        using server_const_iterator = typename server_array::const_iterator;
        using client_ptr = std::shared_ptr<socket_pipeline_client>;
        using client_table = std::unordered_map<sys::socket_address,client_ptr>;
        using client_iterator = typename client_table::iterator;
        using id_type = sbn::kernel::id_type;
        using resource_array = sbn::resources::Bindings;
        using hierarchy_node_array = std::vector<hierarchy_node>;
        using hierarchy_type = Hierarchy<ip_address>;

    private:
        server_array _servers;
        client_table _clients;
        sys::port_type _port = 33333;
        std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);
        socket_pipeline_scheduler _scheduler;
        bool _route = false;

    public:

        explicit socket_pipeline(const properties& p);
        socket_pipeline() = default;
        ~socket_pipeline() = default;
        socket_pipeline(const socket_pipeline&) = delete;
        socket_pipeline(socket_pipeline&&) = delete;
        socket_pipeline& operator=(const socket_pipeline&) = delete;
        socket_pipeline& operator=(socket_pipeline&&) = delete;

        void add_client(const sys::socket_address& addr, const hierarchy_type& hierarchy);
        void stop_client(const sys::socket_address& addr);
        void update_clients(const hierarchy_type& hierarchy);
        void add_server(const interface_address& rhs) {
            this->add_server(sys::ipv4_socket_address(rhs.address(), this->_port), rhs.netmask());
        }
        void add_server(const sys::socket_address& rhs, ip_address netmask);

        void forward(sbn::kernel_ptr&& hdr) override;

        inline void port(sys::port_type rhs) noexcept { this->_port = rhs; }
        inline sys::port_type port() const noexcept { return this->_port; }
        inline const server_array& servers() const noexcept { return this->_servers; }
        inline const client_table& clients() const noexcept { return this->_clients; }
        void remove_server(const interface_address& interface_address);
        inline const socket_pipeline_scheduler& scheduler() const noexcept { return this->_scheduler; }
        inline socket_pipeline_scheduler& scheduler() noexcept { return this->_scheduler; }
        inline bool route() const noexcept { return this->_route; }
        inline void route(bool rhs) noexcept { this->_route = rhs; }

    private:

        void remove_client(const sys::socket_address& vaddr);
        void remove_client(client_iterator result);
        void remove_server(server_iterator result);

        server_iterator
        find_server(const interface_address& interface_address);

        server_iterator
        find_server(sys::fd_type fd);

        server_iterator
        find_server(const sys::socket_address& dest);

        //void ensure_identity(sbn::kernel* k, const sys::socket_address& dest);

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

        friend class socket_pipeline_server;
        friend class socket_pipeline_client;

    };

    class socket_pipeline_client: public sbn::connection {

    public:
        using counter_type = socket_pipeline::counter_type;
        using counter_array = std::array<counter_type,2>;
        using kernel_queue = std::deque<sbn::kernel_ptr>;
        using resource_array = sbn::resources::Bindings;
        using hierarchy_node_array = std::vector<hierarchy_node>;

    private:
        sys::socket _socket;
        sys::socket_address _old_bind_address;
        hierarchy_node_array _nodes_behind;
        counter_type _sum_thread_concurrency{1};
        bool _route = false;

    public:

        inline explicit socket_pipeline_client(sys::socket&& socket):
        _socket(std::move(socket)) {}

        virtual ~socket_pipeline_client() { recover(); }

        socket_pipeline_client() = default;
        socket_pipeline_client& operator=(const socket_pipeline_client&) = delete;
        socket_pipeline_client& operator=(socket_pipeline_client&&) = delete;
        socket_pipeline_client(const socket_pipeline_client&) = delete;
        socket_pipeline_client(socket_pipeline_client&& rhs) = delete;

        inline sbn::kernel_ptr forward(sbn::kernel_ptr k) {
            Expects(k);
            if (k->routed()){
                k->old_id(k->id());
                generate_new_id(k.get());
                k->routed(false);
            }
            return do_forward(std::move(k));
        }

        void recover();
        void handle(const sys::epoll_event& event) override;

        inline void flush() override { connection::flush(this->_socket); }

        inline const sys::socket& socket() const noexcept { return this->_socket; }
        inline sys::socket& socket() noexcept { return this->_socket; }

        void add(const connection_ptr& self) override;
        void remove(const connection_ptr&) override;
        void deactivate(const connection_ptr& self) override;
        void activate(const connection_ptr& self) override;

        using connection::parent;
        inline socket_pipeline* parent() const noexcept {
            return reinterpret_cast<socket_pipeline*>(this->connection::parent());
        }

        inline bool route() const noexcept { return this->_route; }
        inline void route(bool rhs) noexcept { this->_route = rhs; }

        inline const hierarchy_node_array& nodes_behind() const noexcept {
            return this->_nodes_behind;
        }

        inline void nodes_behind(const hierarchy_node_array& rhs) noexcept {
            this->_nodes_behind = rhs;
            update_counters();
        }

        inline void update_counters() {
            using r = sbn::resources::resources;
            counter_type sum = 0;
            for (const auto& n : this->_nodes_behind) {
                sum += n.resources()[r::total_threads].unsigned_integer();
            }
            this->_sum_thread_concurrency = sum;
        }

        inline bool match(sbn::kernel::resource_expression& node_filter) {
            for (const auto& n : this->_nodes_behind) {
                if (node_filter.evaluate(n.resources()).boolean()) {
                    return true;
                }
            }
            return false;
        }

        /*
        inline const resource_array& resources() const noexcept { return this->_resources; }
        inline resource_array& resources() noexcept { return this->_resources; }
        inline void resources(const resource_array& rhs) noexcept { this->_resources = rhs; }
        */

        void receive_foreign_kernel(sbn::kernel_ptr&& k) override;

        /// The number of threads "behind" this node in the hierarchy.
        inline counter_type num_threads_behind() const noexcept {
            return this->_sum_thread_concurrency;
        }

        inline counter_type num_nodes_behind() const noexcept {
            return this->_sum_thread_concurrency;
        }

        /**
          The first element is the number of kernels with the maximum weight sent to the client
          divided by the number of cluster nodes. Each kernel uses all threads of the client.
          The second element is the number of kernels sent to the client divided by
          the number of threads "behind" the client.
        */
        inline sbn::modular_weight_array relative_load() const noexcept {
            sbn::weight_array tmp{load()};
            auto num_nodes = num_nodes_behind();
            if (num_nodes == 0) { num_nodes = 1; }
            //tmp[0] /= num_nodes;
            auto nthreads = num_threads_behind();
            if (nthreads == 0) { nthreads = 1; }
            //tmp[1] /= nthreads;
            return sbn::modular_weight_array{
                {tmp[0]/num_nodes,tmp[0]%num_nodes},
                {tmp[1]/nthreads,tmp[1]%nthreads}};
        }

        void write(std::ostream& out) const override;

    private:

        void receive_downstream_foreign_kernel(sbn::kernel_ptr&& a) {
            Expects(a);
            auto result = find_kernel(a.get(), this->_upstream);
            if (result == this->_upstream.end() || !(*result)->source()) {
                connection::receive_foreign_kernel(std::move(a));
            } else {
                // Route downstream kernel to the node that sent it
                // in upstream phase.
                a->destination((*result)->source());
                auto old = a->id();
                a->id(a->old_id());
                log("change id back _ -> _ kernel _", old, a->id(), *a);
                log("route downstream kernel to _ kernel _", a->destination(), *a);
                this->_upstream.erase(result);
                parent()->forward(std::move(a));
            }
        }

    };

}

#endif // vim:filetype=cpp
