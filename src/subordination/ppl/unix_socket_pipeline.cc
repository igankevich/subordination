#include <subordination/ppl/unix_socket_pipeline.hh>

namespace sbn {

    template <class K, class R>
    class unix_socket_server: public basic_handler {

    private:
        typedef unix_socket_pipeline<K,R> this_type;

    private:
        sys::socket _socket;
        sys::socket_address _socket_address;
        this_type& _ppl;

    public:

        unix_socket_server(const sys::socket_address& endp, this_type& ppl):
        _socket_address(endp),
        _ppl(ppl)
        {
            this->_socket.bind(endp);
            this->_socket.setopt(sys::socket::pass_credentials);
            this->_socket.listen();
            this->log("socket=_", this->_socket);
        }

        void
        handle(const sys::epoll_event& ev) override {
            this->log("_ _", __func__, ev);
            sys::socket_address addr;
            sys::socket sock;
            while (this->_socket.accept(sock, addr)) {
                this->_ppl.add_client(addr, std::move(sock));
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

    };

}

template <class K, class R>
void
sbn::unix_socket_pipeline<K,R>
::add_client(const sys::socket_address& addr, sys::socket&& sock) {
    auto ptr =
        std::make_shared<unix_socket_client<K,R>>(addr, std::move(sock));
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
    #ifndef NDEBUG
    this->log("add _", addr);
    #endif
}

template <class K, class R>
void
sbn::unix_socket_pipeline<K,R>
::add_client(const sys::socket_address& addr) {
    auto ptr = std::make_shared<unix_socket_client<K,R>>(addr);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::inout), ptr);
    this->_clients.emplace(addr, ptr);
}

template <class K, class R>
void
sbn::unix_socket_pipeline<K,R>
::add_server(const sys::socket_address& rhs) {
    auto ptr = std::make_shared<unix_socket_server<K,R>>(rhs, *this);
    this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
}

template <class K, class R> void
sbn::unix_socket_pipeline<K,R>::process_kernels() {
    while (!this->_kernels.empty()) {
        auto* kernel = this->_kernels.front();
        this->_kernels.pop();
        this->process_kernel(kernel);
    }
}

template <class K, class R> void
sbn::unix_socket_pipeline<K,R>::process_kernel(kernel_type* kernel) {
    if (kernel->moves_downstream()) {
        if (!kernel->to()) {
            kernel->to(kernel->from());
        }
        auto client = this->_clients.find(kernel->to());
        if (client != this->_clients.end()) {
            client->second->send(kernel);
        }
    }
}

template class sbn::unix_socket_pipeline<sbn::kernel, sbn::basic_router<sbn::kernel>>;
template class sbn::unix_socket_server<sbn::kernel, sbn::basic_router<sbn::kernel>>;
template class sbn::unix_socket_client<sbn::kernel, sbn::basic_router<sbn::kernel>>;
