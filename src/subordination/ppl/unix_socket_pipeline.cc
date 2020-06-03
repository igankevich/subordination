#include <subordination/ppl/basic_factory.hh>
#include <subordination/ppl/unix_socket_pipeline.hh>

namespace sbn {

    class unix_socket_server: public basic_handler {

    private:

    private:
        sys::socket _socket;
        sys::socket_address _socket_address;
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

        void
        write(std::ostream& out) const override {
            out << "server " << this->_socket_address;
        }

        inline sys::fd_type
        fd() const noexcept {
            return this->_socket.fd();
        }

        inline void socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline void parent(unix_socket_pipeline* rhs) noexcept {
            this->_parent = rhs;
        }

    };

}

sbn::unix_socket_client::unix_socket_client(sys::socket&& sock):
_buffer(new kernelbuf_type),
_stream(_buffer.get()) {
    this->_buffer->setfd(std::move(sock));
    this->setstate(pipeline_state::starting);
}

sbn::unix_socket_pipeline::unix_socket_pipeline() {
    this->_protocol.setf(
        kernel_proto_flag::prepend_application |
        kernel_proto_flag::save_upstream_kernels |
        kernel_proto_flag::save_downstream_kernels
    );
}

void sbn::unix_socket_pipeline::add_client(const sys::socket_address& addr,
                                           sys::socket&& sock) {
    auto ptr = std::make_shared<unix_socket_client>(std::move(sock));
    ptr->socket_address(addr);
    ptr->protocol(&this->_protocol);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
    #ifndef NDEBUG
    this->log("add _", addr);
    #endif
}

void sbn::unix_socket_pipeline::add_client(const sys::socket_address& addr) {
    sys::socket s(sys::family_type::unix);
    s.setopt(sys::socket::pass_credentials);
    s.connect(addr);
    auto ptr = std::make_shared<unix_socket_client>(std::move(s));
    ptr->socket_address(addr);
    ptr->protocol(&this->_protocol);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

void sbn::unix_socket_pipeline::add_server(const sys::socket_address& rhs) {
    sys::socket s(sys::family_type::unix);
    s.bind(rhs);
    s.setopt(sys::socket::pass_credentials);
    s.listen();
    this->log("socket=_", s);
    auto ptr = std::make_shared<unix_socket_server>(std::move(s));
    ptr->parent(this);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
}

void
sbn::unix_socket_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        auto* kernel = this->_kernels.front();
        this->_kernels.pop();
        this->process_kernel(kernel);
    }
}

void
sbn::unix_socket_pipeline::process_kernel(kernel* k) {
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
