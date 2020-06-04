#include <subordination/core/socket_pipeline.hh>

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <unistdx/base/make_object>
#include <unistdx/net/socket>

#include <subordination/core/basic_factory.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/socket_pipeline_event.hh>

namespace {

    template <class kernel, class Server>
    void
    set_kernel_id(kernel* k, Server& srv) {
        if (!k->has_id()) {
            k->set_id(srv.generate_id());
        }
    }

    template <class kernel, class Id>
    void
    set_kernel_id_2(kernel* k, Id& counter) {
        if (!k->has_id()) {
            k->set_id(++counter);
        }
    }

}

namespace sbn {

    class local_server: public basic_handler {

    public:
        using ip_address = sys::ipv4_address;
        using interface_address_type = sys::interface_address<ip_address>;
        using id_type = typename interface_address_type::rep_type;
        using counter_type = id_type;

    private:
        interface_address_type _ifaddr;
        id_type _pos0 = 0;
        id_type _pos1 = 0;
        counter_type _counter {0};
        sys::socket _socket;
        socket_pipeline* _parent;

    public:

        inline explicit
        local_server(const interface_address_type& ifaddr, sys::port_type port):
        _ifaddr(ifaddr),
        _socket(sys::socket_address(ifaddr.address(), port)) {
            socket_address(sys::socket_address(ifaddr.address(), port));
            this->init();
        }

        local_server() = delete;
        local_server(const local_server&) = delete;
        local_server& operator=(const local_server&) = delete;
        local_server& operator=(local_server&&) = default;

        inline const interface_address_type
        interface_address() const noexcept {
            return this->_ifaddr;
        }

        inline sys::fd_type fd() const noexcept { return this->_socket.fd(); }
        inline const sys::socket& socket() const noexcept { return this->_socket; }
        inline sys::socket& socket() noexcept { return this->_socket; }

        inline id_type
        generate_id() noexcept {
            id_type id;
            if (this->_counter == this->_pos1) {
                id = this->_pos0;
                this->_counter = id+1;
            } else {
                id = this->_counter++;
            }
            return id;
        }

        void handle(const sys::epoll_event& ev) override;

        void
        remove(sys::event_poller& poller) override {
            poller.erase(this->fd());
            this->_parent->remove_server(this->_ifaddr);
        }

        inline void parent(socket_pipeline* rhs) noexcept {
            basic_handler::parent(rhs);
            this->_parent = rhs;
        }

    private:

        inline void
        init() noexcept {
            determine_id_range(this->_ifaddr, this->_pos0, this->_pos1);
            this->_counter = this->_pos0;
        }

    };

    class remote_client: public basic_handler {

    public:
        using fildesbuf_type = sys::basic_fildesbuf<char, std::char_traits<char>, sys::socket>;
        using kernelbuf_type = basic_kernelbuf<fildesbuf_type>;
        using kernelbuf_ptr = std::unique_ptr<kernelbuf_type>;
        using weight_type = typename socket_pipeline::weight_type;

    private:
        kernelbuf_ptr _buffer;
        kstream _stream;
        /// The number of nodes "behind" this one in the hierarchy.
        weight_type _weight = 1;
        socket_pipeline* _parent = nullptr;

    public:

        inline explicit remote_client(sys::socket&& sock):
        basic_handler(&this->_stream), _buffer(new kernelbuf_type),
        _stream(this->_buffer.get()) {
            this->_buffer->setfd(std::move(sock));
        }
        virtual ~remote_client() { recover(); }

        remote_client() = default;
        remote_client& operator=(const remote_client&) = delete;
        remote_client& operator=(remote_client&&) = delete;
        remote_client(const remote_client&) = delete;
        remote_client(remote_client&& rhs) = delete;

        void recover() {
            // Here failed kernels are written to buffer,
            // from which they must be recovered with recover_kernels().
            sys::epoll_event ev {socket().fd(), sys::event::in};
            this->handle(ev);
            // recover kernels from upstream and downstream buffer
            recover_kernels(true);
        }

        void
        handle(const sys::epoll_event& event) override {
            if (this->is_starting() && !event.err()) {
                this->setstate(pipeline_state::started);
            }
            if (event.in()) {
                if (this->_buffer->is_safe_to_compact()) {
                    this->_buffer->compact();
                }
                this->_buffer->pubfill();
                receive_kernels();
            }
        }

        void
        flush() override {
            if (this->_buffer->dirty()) {
                this->_buffer->pubflush();
            }
        }

        inline const sys::socket& socket() const noexcept { return this->_buffer->fd(); }
        inline sys::socket& socket() noexcept { return this->_buffer->fd(); }

        void
        socket(sys::socket&& rhs) {
            this->_buffer->pubsync();
            this->_buffer->setfd(sys::socket(std::move(rhs)));
        }

        inline weight_type
        weight() const noexcept {
            return this->_weight;
        }

        inline void
        weight(weight_type rhs) noexcept {
            this->_weight = rhs;
        }

        inline void
        name(const char* rhs) noexcept {
            this->pipeline_base::name(rhs);
            #if defined(SBN_DEBUG)
            if (this->_buffer) {
                this->_buffer->set_name(rhs);
            }
            #endif
        }

        void
        remove(sys::event_poller& poller) override {
            poller.erase(this->_buffer->fd().fd());
            this->_parent->remove_client(this->socket_address());
        }

        inline void parent(socket_pipeline* rhs) noexcept {
            basic_handler::parent(rhs);
            this->_parent = rhs;
        }

        inline socket_pipeline* parent() const noexcept { return this->_parent; }

    };

}

void sbn::local_server::handle(const sys::epoll_event& ev) {
    sys::socket_address addr;
    sys::socket sock;
    while (this->_socket.accept(sock, addr)) {
        sys::socket_address vaddr = this->_parent->virtual_addr(addr);
        auto res = this->_parent->_clients.find(vaddr);
        if (res == this->_parent->_clients.end()) {
            #if defined(SBN_DEBUG)
            auto ptr =
            #endif
            this->_parent->do_add_client(std::move(sock), vaddr);
            #if defined(SBN_DEBUG)
            this->log("accept _", ptr->socket_address());
            #endif
        }
    }
}

sbn::socket_pipeline
::socket_pipeline() {
    using namespace std::chrono;
    this->set_start_timeout(seconds(7));
}

void
sbn::socket_pipeline
::remove_server(const interface_address& interface_address) {
    lock_type lock(this->_mutex);
    server_iterator result = this->find_server(interface_address);
    if (result != this->_servers.end()) {
        this->remove_server(result);
    }
}

void
sbn::socket_pipeline
::remove_server(server_iterator result) {
    // copy interface_address
    interface_address interface_address = (*result)->interface_address();
    this->_servers.erase(result);
    fire_event_kernels(
        new socket_pipeline_kernel(socket_pipeline_event::remove_server, interface_address));
}

void
sbn::socket_pipeline
::remove_client(const sys::socket_address& vaddr) {
    this->log("remove client _", vaddr);
    client_iterator result = this->_clients.find(vaddr);
    if (result != this->_clients.end()) {
        this->remove_client(result);
    }
}

void
sbn::socket_pipeline
::remove_client(client_iterator result) {
    // copy socket_address
    sys::socket_address socket_address = result->first;
    #if defined(SBN_DEBUG)
    const char* reason =
        result->second->is_starting() ? "timed out" : "connection closed";
    this->log("remove client _ (_)", socket_address, reason);
    #endif
    if (result == this->_iterator) {
        this->advance_client_iterator();
    }
    result->second->setstate(pipeline_state::stopped);
    this->_clients.erase(result);
    fire_event_kernels(
        new socket_pipeline_kernel(socket_pipeline_event::remove_client, socket_address));
}

void
sbn::socket_pipeline
::add_server(const sys::socket_address& rhs, ip_address netmask) {
    using traits_type = sys::ipaddr_traits<ip_address>;
    lock_type lock(this->_mutex);
    interface_address interface_address(traits_type::address(rhs), netmask);
    if (this->find_server(interface_address) == this->_servers.end()) {
        auto ptr = std::make_shared<local_server>(interface_address, traits_type::port(rhs));
        ptr->parent(this);
        this->_servers.emplace_back(ptr);
        #if defined(SBN_DEBUG)
        this->log("add server _", rhs);
        #endif
        ptr->socket().set_user_timeout(this->_socket_timeout);
        this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
        fire_event_kernels(
            new socket_pipeline_kernel(socket_pipeline_event::add_server, interface_address));
    }
}

void sbn::socket_pipeline::forward(foreign_kernel* hdr) {
    // do not lock here as static_lock locks both mutexes
    assert(this->other_mutex());
    assert(hdr->is_foreign());
    if (hdr->to()) {
        auto ptr = this->find_or_create_client(hdr->to());
        #if defined(SBN_DEBUG)
        this->log("fwd _ to _", *hdr, hdr->to());
        #endif
        ptr->forward(hdr);
        this->_semaphore.notify_one();
    } else {
        if (this->end_reached() && hdr->moves_upstream() && hdr->carries_parent()) {
            this->find_next_client();
            if (this->end_reached()) {
                this->log("forwarding kernel carrying parent to localhost _", *hdr);
            }
        }
        if (this->end_reached()) {
            this->find_next_client();
            #if defined(SBN_DEBUG)
            this->log("fwd _ to _", *hdr, "localhost");
            #endif
            foreign_pipeline()->forward(hdr);
        } else {
            #if defined(SBN_DEBUG)
            this->log("fwd _ to _", *hdr, this->current_client().socket_address());
            #endif
            this->current_client().forward(hdr);
            this->find_next_client();
            this->_semaphore.notify_one();
        }
    }
}

typename sbn::socket_pipeline::server_iterator
sbn::socket_pipeline
::find_server(const interface_address& interface_address) {
    typedef typename server_array::value_type value_type;
    return std::find_if(
        this->_servers.begin(),
        this->_servers.end(),
        [&interface_address] (const value_type& rhs) {
            return rhs->interface_address() == interface_address;
        }
    );
}

typename sbn::socket_pipeline::server_iterator
sbn::socket_pipeline
::find_server(sys::fd_type fd) {
    typedef typename server_array::value_type value_type;
    return std::find_if(
        this->_servers.begin(),
        this->_servers.end(),
        [fd] (const value_type& rhs) {
            return rhs->socket().fd() == fd;
        }
    );
}

typename sbn::socket_pipeline::server_iterator
sbn::socket_pipeline
::find_server(const sys::socket_address& dest) {
    typedef typename server_array::value_type value_type;
    return std::find_if(
        this->_servers.begin(),
        this->_servers.end(),
        [&dest] (const value_type& rhs) {
            return rhs->interface_address().contains(dest.addr4());
        }
    );
}

void
sbn::socket_pipeline
::ensure_identity(kernel* k, const sys::socket_address& dest) {
    if (dest.family() == sys::family_type::unix) {
        set_kernel_id_2(k, this->_counter);
        set_kernel_id_2(k->parent(), this->_counter);
    } else {
        if (this->_servers.empty()) {
            k->return_to_parent(
                exit_code::no_upstream_servers_available
            );
        } else {
            server_iterator result = this->find_server(dest);
            if (result == this->_servers.end()) {
                result = this->_servers.begin();
            }
            set_kernel_id(k, **result);
            set_kernel_id(k->parent(), **result);
        }
    }
}

void
sbn::socket_pipeline
::find_next_client() {
    if (this->_clients.empty()) {
        return;
    }
    client_iterator old_iterator = this->_iterator;
    do {
        // wrap iterator
        if (this->end_reached()) {
            this->_iterator = this->_clients.begin();
        }
        // increment
        if (!this->end_reached()) {
            if (this->_weightcnt < this->current_client().weight()) {
                ++this->_weightcnt;
            } else {
                this->advance_client_iterator();
            }
        }
        // include localhost into round-robin
        if (this->_uselocalhost && this->end_reached()) {
            break;
        }
        // skip stopped hosts
        if (!this->end_reached() && this->current_client().has_started()) {
            break;
        }
    } while (old_iterator != this->_iterator);
}

void
sbn::socket_pipeline::emplace_client(const sys::socket_address& vaddr, const client_ptr& s) {
    const bool save = !this->end_reached();
    sys::socket_address e;
    if (save) {
        e = this->_iterator->first;
    }
    this->_clients.emplace(vaddr, s);
    if (save) {
        this->_iterator = this->_clients.find(e);
    } else {
        this->reset_iterator();
    }
}

void
sbn::socket_pipeline
::process_kernels() {
//	lock_type lock(this->_mutex);
    while (!this->_kernels.empty()) {
        auto* k = this->_kernels.front();
        this->_kernels.pop();
        try {
            this->process_kernel(k);
        } catch (const std::exception& err) {
            this->log_error(err);
            k->from(k->to());
            k->return_to_parent(exit_code::no_upstream_servers_available);
            native_pipeline()->send(k);
        }
    }
}

void
sbn::socket_pipeline
::process_kernel(kernel* k) {
    // short circuit local server
    /*
       if (k->to()) {
        server_iterator result = this->find_server(k->to());
        if (result != this->_servers.end()) {
            k->from(k->to());
            k->return_to_parent(exit_code::no_upstream_servers_available);
            native_pipeline()->send(k);
            return;
        }
       }
     */
    if (k->moves_everywhere()) {
        for (auto& pair : _clients) {
            pair.second->send(k);
        }
        // delete broadcast kernel
        delete k;
    } else if (k->moves_upstream() && k->to() == sys::socket_address()) {
        bool success = false;
        if (this->_uselocalhost && !k->carries_parent()) {
            if (this->end_reached()) {
                // include localhost in round-robin
                // (short-circuit kernels when no upstream servers
                // are available)
                native_pipeline()->send(k);
            } else {
                success = true;
            }
        } else {
            if (this->_clients.empty()) {
                k->return_to_parent(exit_code::no_upstream_servers_available);
                native_pipeline()->send(k);
            } else {
                success = true;
            }
        }
        if (success) {
            ensure_identity(k, this->_iterator->second->socket_address());
            this->_iterator->second->send(k);
        }
        this->find_next_client();
    } else if (k->moves_downstream() and not k->from()) {
        // kernel @k was sent to local node
        // because no upstream servers had
        // been available
        native_pipeline()->send(k);
    } else {
        // create socket_address if necessary, and send kernel
        if (not k->to()) {
            k->to(k->from());
        }
        if (k->moves_somewhere()) {
            ensure_identity(k, k->to());
        }
        this->find_or_create_client(k->to())->send(k);
    }
}

auto
sbn::socket_pipeline::find_or_create_client(const sys::socket_address& addr) -> client_ptr {
    client_ptr ret;
    auto result = _clients.find(addr);
    if (result == _clients.end()) {
        ret = this->do_add_client(addr);
    } else {
        ret = result->second;
    }
    return ret;
}

auto
sbn::socket_pipeline::do_add_client(const sys::socket_address& addr) -> client_ptr {
    if (addr.family() == sys::family_type::unix) {
        sys::socket s(sys::family_type::unix);
        s.setopt(sys::socket::pass_credentials);
        s.connect(addr);
        return this->do_add_client(std::move(s), addr);
    } else {
        server_iterator result = this->find_server(addr);
        if (result == this->_servers.end()) {
            throw std::invalid_argument("no matching server found");
        }
        // bind to server address with ephemeral port
        sys::socket_address srv_addr((*result)->socket_address(), 0);
        sys::socket s(addr.family());
        s.bind(srv_addr);
        try {
            s.connect(addr);
        } catch (const sys::bad_call& err) {
            if (err.errc() != std::errc::connection_refused) {
                throw;
            }
        }
        return this->do_add_client(std::move(s), addr);
    }
}

auto
sbn::socket_pipeline::do_add_client(sys::socket&& sock, sys::socket_address vaddr) -> client_ptr {
    sys::fd_type fd = sock.fd();
    if (vaddr.family() != sys::family_type::unix) {
        sock.set_user_timeout(this->_socket_timeout);
    }
    auto s = std::make_shared<remote_client>(std::move(sock));
    s->socket_address(vaddr);
    s->parent(this);
    s->setstate(pipeline_state::starting);
    s->name(this->_name);
    using f = kernel_proto_flag;
    s->setf(f::prepend_application | f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_client(vaddr, s);
    this->emplace_handler(sys::epoll_event(fd, sys::event::inout), s);
    fire_event_kernels(new socket_pipeline_kernel(socket_pipeline_event::add_client, vaddr));
    return s;
}

void
sbn::socket_pipeline
::stop_client(const sys::socket_address& addr) {
    lock_type lock(this->_mutex);
    auto result = this->_clients.find(addr);
    if (result != this->_clients.end()) {
        result->second->setstate(pipeline_state::stopped);
    }
}

void
sbn::socket_pipeline
::set_client_weight(
    const sys::socket_address& addr,
    weight_type new_weight
) {
    lock_type lock(this->_mutex);
    client_iterator result = this->_clients.find(addr);
    if (result != this->_clients.end()) {
        result->second->weight(new_weight);
    }
}

void
sbn::socket_pipeline
::print_state(std::ostream& out) {
    lock_type lock(this->_mutex);
    for (const auto& val : this->_servers) {
        this->log("server _", val);
    }
    for (const auto& val : this->_clients) {
        this->log("client _, handler _", val.first, val.second->socket_address());
    }
}

void sbn::socket_pipeline::fire_event_kernels(socket_pipeline_kernel* ev) {
    instances_guard g(instances);
    for (const auto& pair : instances) {
        ev->parent(ev);
        ev->principal(pair.second);
        native_pipeline()->send(ev);
    }
}
