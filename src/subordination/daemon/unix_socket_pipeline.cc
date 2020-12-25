#include <subordination/bits/contracts.hh>
#include <subordination/daemon/unix_socket_pipeline.hh>

namespace sbnd {

    class unix_socket_server: public sbn::connection {

    private:
        sys::socket _socket;

    public:

        inline explicit unix_socket_server(sys::socket&& s): _socket(std::move(s)) {}

        void
        handle(const sys::epoll_event& ev) override {
            sys::socket_address addr;
            sys::socket sock;
            while (this->_socket.accept(sock, addr)) {
                parent()->add_client(
                    sys::socket_address_cast<sys::unix_socket_address>(addr), std::move(sock));
            }
        };

        inline sys::fd_type
        fd() const noexcept {
            return this->_socket.fd();
        }

        using connection::parent;
        inline unix_socket_pipeline* parent() const noexcept {
            return reinterpret_cast<unix_socket_pipeline*>(this->connection::parent());
        }

    };

}

void sbnd::unix_socket_pipeline::add_client(const sys::unix_socket_address& addr,
                                            sys::socket&& sock) {
    Expects(bool(addr));
    using f = sbn::connection_flags;
    auto ptr = std::make_shared<unix_socket_client>(std::move(sock));
    ptr->name(name());
    ptr->parent(this);
    ptr->types(types());
    ptr->socket_address(addr);
    ptr->setf(f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbnd::unix_socket_pipeline::add_client(const sys::unix_socket_address& addr) {
    Expects(bool(addr));
    using f = sbn::connection_flags;
    sys::socket s(sys::family_type::unix);
    s.set(sys::socket::options::pass_credentials);
    try {
        s.connect(addr);
    } catch (const sys::bad_call& err) {
        if (err.errc() != std::errc::connection_refused) {
            throw;
        }
    }
    auto ptr = std::make_shared<unix_socket_client>(std::move(s));
    ptr->name(name());
    ptr->parent(this);
    ptr->types(types());
    ptr->socket_address(addr);
    ptr->setf(f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbnd::unix_socket_pipeline::add_server(const sys::unix_socket_address& rhs) {
    Expects(bool(rhs));
    sys::socket s(sys::family_type::unix);
    s.set(sys::socket::options::reuse_address);
    s.set(sys::socket::options::pass_credentials);
    s.bind(rhs);
    s.listen();
    auto ptr = std::make_shared<unix_socket_server>(std::move(s));
    ptr->name(name());
    ptr->parent(this);
    ptr->socket_address(rhs);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
}

void sbnd::unix_socket_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto kernel = std::move(this->_kernels.front());
        this->_kernels.pop_front();
        this->process_kernel(std::move(kernel));
    }
}

void sbnd::unix_socket_pipeline::process_kernel(sbn::kernel_ptr&& k) {
    Expects(k.get());
    if (k->phase() == sbn::kernel::phases::downstream) {
        if (!k->destination()) {
            k->destination(k->source());
        }
        auto client = this->_clients.find(k->destination());
        if (client != this->_clients.end()) {
            client->second->send(k);
        }
    }
}


void sbnd::unix_socket_pipeline::forward(sbn::foreign_kernel_ptr&& fk) {
    Expects(fk.get());
    auto result = this->_clients.find(fk->destination());
    if (result == this->_clients.end()) {
        log("client _ not found, deleting _", fk->destination(), *fk);
        return;
    }
    result->second->forward(std::move(fk));
    this->_semaphore.notify_one();
}

sbnd::unix_socket_client::unix_socket_client(sys::socket&& socket):
_socket(std::move(socket)) {
    this->state(sbn::connection_state::starting);
}

void sbnd::unix_socket_client::receive_kernel(sbn::kernel_ptr&& k) {
    Expects(k.get());
    if (auto* a = k->source_application()) {
        a->credentials(socket().credentials());
    }
    if (auto* a = k->target_application()) {
        a->credentials(socket().credentials());
    }
    connection::receive_kernel(std::move(k));
}

void sbnd::unix_socket_client::handle(const sys::epoll_event& event) {
    if (state() == sbn::connection_state::starting) { state(sbn::connection_state::started); }
    if (event.in()) {
        fill(this->_socket);
        receive_kernels();
    }
};

void sbnd::unix_socket_client::flush() { connection::flush(this->_socket); }
