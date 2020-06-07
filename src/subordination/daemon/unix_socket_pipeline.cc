#include <subordination/daemon/unix_socket_pipeline.hh>

namespace sbnd {

    class unix_socket_server: public sbn::connection {

    private:
        sys::socket _socket;

    public:

        inline explicit unix_socket_server(sys::socket&& s): _socket(std::move(s)) {}

        void
        handle(const sys::epoll_event& ev) override {
            this->log("_ _", __func__, ev);
            sys::socket_address addr;
            sys::socket sock;
            while (this->_socket.accept(sock, addr)) {
                parent()->add_client(addr, std::move(sock));
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

void sbnd::unix_socket_pipeline::add_client(const sys::socket_address& addr,
                                           sys::socket&& sock) {
    using f = sbn::kernel_proto_flag;
    auto ptr = std::make_shared<unix_socket_client>(std::move(sock));
    ptr->name(name());
    ptr->parent(this);
    ptr->socket_address(addr);
    ptr->setf(f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbnd::unix_socket_pipeline::add_client(const sys::socket_address& addr) {
    using f = sbn::kernel_proto_flag;
    sys::socket s(sys::family_type::unix);
    s.setopt(sys::socket::pass_credentials);
    s.connect(addr);
    auto ptr = std::make_shared<unix_socket_client>(std::move(s));
    ptr->name(name());
    ptr->parent(this);
    ptr->socket_address(addr);
    ptr->setf(f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbnd::unix_socket_pipeline::add_server(const sys::socket_address& rhs) {
    sys::socket s(sys::family_type::unix);
    s.bind(rhs);
    s.setopt(sys::socket::pass_credentials);
    s.listen();
    auto ptr = std::make_shared<unix_socket_server>(std::move(s));
    ptr->name(name());
    ptr->parent(this);
    ptr->socket_address(rhs);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
}

void sbnd::unix_socket_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto* kernel = this->_kernels.front();
        this->_kernels.pop();
        this->process_kernel(kernel);
    }
}

void
sbnd::unix_socket_pipeline::process_kernel(sbn::kernel* k) {
    if (k->moves_downstream()) {
        if (!k->destination()) {
            k->destination(k->source());
        }
        auto client = this->_clients.find(k->destination());
        if (client != this->_clients.end()) {
            client->second->send(k);
        }
    }
}

sbnd::unix_socket_client::unix_socket_client(sys::socket&& socket):
_socket(std::move(socket)) {
    this->setstate(sbn::pipeline_state::starting);
}

void sbnd::unix_socket_client::handle(const sys::epoll_event& event) {
    this->log("_ _", __func__, event);
    if (this->is_starting()) { this->setstate(sbn::pipeline_state::started); }
    if (event.in()) {
        fill(this->_socket);
        receive_kernels(
            nullptr,
            [this] (sbn::kernel* k) {
                if (auto* app = dynamic_cast<application_kernel*>(k)) {
                    app->credentials(socket().credentials());
                }
            });
    }
};

void sbnd::unix_socket_client::flush() { connection::flush(this->_socket); }
