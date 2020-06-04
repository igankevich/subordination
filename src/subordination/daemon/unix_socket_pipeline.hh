#ifndef SUBORDINATION_DAEMON_UNIX_SOCKET_PIPELINE_HH
#define SUBORDINATION_DAEMON_UNIX_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <unistdx/io/fildesbuf>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/kernel_protocol.hh>
#include <subordination/core/kstream.hh>
#include <subordination/core/types.hh>
#include <subordination/daemon/application_kernel.hh>

namespace sbnd {

    class unix_socket_client: public sbn::basic_handler {

    private:
        using kernelbuf_type =
            sbn::basic_kernelbuf<sys::basic_fildesbuf<char,std::char_traits<char>,sys::socket>>;
        using kernelbuf_ptr = std::unique_ptr<kernelbuf_type>;
        using stream_type = sbn::kstream;
        using ipacket_guard = typename stream_type::ipacket_guard;
        using opacket_guard = sys::opacket_guard<stream_type>;

    private:
        sys::socket_address _socket_address;
        kernelbuf_ptr _buffer;
        stream_type _stream;
        sbn::kernel_protocol* _protocol = nullptr;

    public:

        explicit unix_socket_client(sys::socket&& sock);

        inline void send(sbn::kernel* k) {
            this->_protocol->send(k, this->_stream, this->_socket_address);
        }

        void
        handle(const sys::epoll_event& event) override {
            this->log("_ _", __func__, event);
            if (this->is_starting()) { this->setstate(sbn::pipeline_state::started); }
            if (event.in()) {
                if (this->_buffer->is_safe_to_compact()) { this->_buffer->compact(); }
                this->_buffer->pubfill();
                this->_protocol->receive_kernels(
                    this->_stream,
                    this->_socket_address,
                    nullptr,
                    [this] (sbn::kernel* k) {
                        if (auto* app = dynamic_cast<application_kernel*>(k)) {
                            app->credentials(socket().credentials());
                        }
                    });
            }
        };

        void flush() override {
            if (this->_buffer->dirty()) {
                this->_buffer->pubflush();
            }
        }

        void
        write(std::ostream& out) const override {
            out << "client " << this->_socket_address << ' ' << this->_buffer->fd();
        }

        inline sys::fd_type fd() const noexcept { return this->_buffer->fd().fd(); }
        inline const sys::socket& socket() const noexcept { return this->_buffer->fd(); }

        inline void
        name(const char* rhs) noexcept {
            this->pipeline_base::name(rhs);
            this->_protocol->name(rhs);
            #if defined(SBN_DEBUG)
            if (this->_buffer) {
                this->_buffer->set_name(rhs);
            }
            #endif
        }

        inline void socket_address(const sys::socket_address& rhs) noexcept {
            this->_socket_address = rhs;
        }

        inline void protocol(sbn::kernel_protocol* rhs) noexcept { this->_protocol = rhs; }

    };

    class unix_socket_pipeline: public sbn::basic_socket_pipeline {

    public:
        using client_type = unix_socket_client;
        using client_ptr = std::shared_ptr<client_type>;
        using client_table = std::unordered_map<sys::socket_address,client_ptr>;

    private:
        client_table _clients;

    public:
        unix_socket_pipeline();
        ~unix_socket_pipeline() = default;
        unix_socket_pipeline(const unix_socket_pipeline& rhs) = delete;
        unix_socket_pipeline(unix_socket_pipeline&& rhs) = delete;

        void add_server(const sys::socket_address& rhs);
        void add_client(const sys::socket_address& addr);

    private:

        void add_client(const sys::socket_address& addr, sys::socket&& sock);

        void process_kernels() override;
        void process_kernel(sbn::kernel* k);

        friend class unix_socket_server;
        friend class unix_socket_client;

    };

}

#endif // vim:filetype=cpp
