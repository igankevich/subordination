#include <subordination/daemon/unix_socket_pipeline.hh>

namespace sbnd {

    class unix_socket_server: public sbn::connection {

    private:

    private:
        sys::socket _socket;
        unix_socket_pipeline* _parent = nullptr;

    public:

        inline explicit unix_socket_server(sys::socket&& s): _socket(std::move(s)) {}

        void
        handle(const sys::epoll_event& ev) override {
            this->log("_ _", __func__, ev);
            sys::socket_address addr;
            sys::socket sock;
            while (this->_socket.accept(sock, addr)) {
                this->_parent->add_client(addr, std::move(sock));
            }
        };

        inline sys::fd_type
        fd() const noexcept {
            return this->_socket.fd();
        }

        inline void parent(unix_socket_pipeline* rhs) noexcept {
            connection::parent(rhs);
            this->_parent = rhs;
        }

    };

}

void sbnd::unix_socket_pipeline::add_client(const sys::socket_address& addr,
                                           sys::socket&& sock) {
    using f = sbn::kernel_proto_flag;
    auto ptr = std::make_shared<unix_socket_client>(std::move(sock));
    ptr->parent(this);
    ptr->socket_address(addr);
    ptr->setf(f::prepend_application | f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
    #if defined(SBN_DEBUG)
    this->log("add _", addr);
    #endif
}

void sbnd::unix_socket_pipeline::add_client(const sys::socket_address& addr) {
    using f = sbn::kernel_proto_flag;
    sys::socket s(sys::family_type::unix);
    s.setopt(sys::socket::pass_credentials);
    s.connect(addr);
    auto ptr = std::make_shared<unix_socket_client>(std::move(s));
    ptr->socket_address(addr);
    ptr->setf(f::prepend_application | f::save_upstream_kernels | f::save_downstream_kernels);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbnd::unix_socket_pipeline::add_server(const sys::socket_address& rhs) {
    sys::socket s(sys::family_type::unix);
    s.bind(rhs);
    s.setopt(sys::socket::pass_credentials);
    s.listen();
    this->log("socket=_", s);
    auto ptr = std::make_shared<unix_socket_server>(std::move(s));
    ptr->parent(this);
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
        if (!k->to()) {
            k->to(k->from());
        }
        auto client = this->_clients.find(k->to());
        if (client != this->_clients.end()) {
            client->second->send(k);
        }
    }
}

sbnd::unix_socket_client::unix_socket_client(sys::socket&& sock):
connection(&this->_stream),
_buffer(new kernelbuf_type), _stream(this->_buffer.get()) {
    this->_buffer->setfd(std::move(sock));
    this->setstate(sbn::pipeline_state::starting);
}

void sbnd::unix_socket_client::handle(const sys::epoll_event& event) {
    this->log("_ _", __func__, event);
    if (this->is_starting()) { this->setstate(sbn::pipeline_state::started); }
    if (event.in()) {
        if (this->_buffer->is_safe_to_compact()) { this->_buffer->compact(); }
        this->_buffer->pubfill();
        receive_kernels(
            nullptr,
            [this] (sbn::kernel* k) {
                if (auto* app = dynamic_cast<application_kernel*>(k)) {
                    app->credentials(socket().credentials());
                }
            });
    }
};

void sbnd::unix_socket_client::flush() {
    if (this->_buffer->dirty()) {
        this->_buffer->pubflush();
    }
}
