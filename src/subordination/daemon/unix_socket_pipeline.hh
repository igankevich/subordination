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
#include <subordination/core/kstream.hh>
#include <subordination/core/types.hh>
#include <subordination/daemon/application_kernel.hh>

namespace sbnd {

    class unix_socket_client: public sbn::connection {

    private:
        using kernelbuf_type =
            sbn::basic_kernelbuf<sys::basic_fildesbuf<char,std::char_traits<char>,sys::socket>>;
        using kernelbuf_ptr = std::unique_ptr<kernelbuf_type>;

    private:
        kernelbuf_ptr _buffer;
        sbn::kstream _stream;

    public:

        explicit unix_socket_client(sys::socket&& sock);

        void handle(const sys::epoll_event& event) override;
        void flush() override;

        inline sys::fd_type fd() const noexcept { return this->_buffer->fd().fd(); }
        inline const sys::socket& socket() const noexcept { return this->_buffer->fd(); }

        inline void
        name(const char* rhs) noexcept {
            this->pipeline_base::name(rhs);
            #if defined(SBN_DEBUG)
            if (this->_buffer) {
                this->_buffer->set_name(rhs);
            }
            #endif
        }

    };

    class unix_socket_pipeline: public sbn::basic_socket_pipeline {

    public:
        using client_type = unix_socket_client;
        using client_ptr = std::shared_ptr<client_type>;
        using client_table = std::unordered_map<sys::socket_address,client_ptr>;

    private:
        client_table _clients;

    public:
        unix_socket_pipeline() = default;
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
