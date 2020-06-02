#ifndef SUBORDINATION_PPL_UNIX_SOCKET_PIPELINE_HH
#define SUBORDINATION_PPL_UNIX_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <unistdx/io/fildesbuf>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <subordination/kernel/kstream.hh>
#include <subordination/ppl/application_kernel.hh>
#include <subordination/ppl/basic_router.hh>
#include <subordination/ppl/basic_socket_pipeline.hh>
#include <subordination/ppl/kernel_protocol.hh>

namespace sbn {

    template <class K, class R>
    class unix_socket_client: public basic_handler {

    public:
        using kernel_type = K;
        using router_type = R;

    private:
        using kernelbuf_type =
            basic_kernelbuf<sys::basic_fildesbuf<char,std::char_traits<char>,sys::socket>>;
        using kernelbuf_ptr = std::unique_ptr<kernelbuf_type>;
        using stream_type = kstream<kernel_type>;
        using ipacket_guard = typename stream_type::ipacket_guard;
        using opacket_guard = sys::opacket_guard<stream_type>;
        using protocol_type = kernel_protocol<K,R,bits::forward_to_child<R>>;

    private:
        sys::socket_address _socket_address;
        kernelbuf_ptr _buffer;
        stream_type _stream;
        protocol_type _proto;

    public:

        inline explicit
        unix_socket_client(const sys::socket_address& endp):
        _socket_address(endp),
        _buffer(new kernelbuf_type),
        _stream(_buffer.get()) {
            this->_proto.setf(
                kernel_proto_flag::prepend_application |
                kernel_proto_flag::save_upstream_kernels |
                kernel_proto_flag::save_downstream_kernels
            );
            this->_proto.socket_address(this->_socket_address);
            sys::socket s(sys::family_type::unix);
            s.setopt(sys::socket::pass_credentials);
            s.connect(endp);
            this->_buffer->setfd(std::move(s));
            this->setstate(pipeline_state::starting);
        }

        inline explicit
        unix_socket_client(const sys::socket_address& endp, sys::socket&& sock):
        _socket_address(endp),
        _buffer(new kernelbuf_type),
        _stream(_buffer.get()) {
            this->_proto.setf(
                kernel_proto_flag::prepend_application |
                kernel_proto_flag::save_upstream_kernels |
                kernel_proto_flag::save_downstream_kernels
            );
            this->_proto.socket_address(this->_socket_address);
            this->_buffer->setfd(std::move(sock));
            this->setstate(pipeline_state::starting);
        }

        inline void send(kernel_type* kernel) { this->_proto.send(kernel, this->_stream); }

        void
        handle(const sys::epoll_event& event) override {
            this->log("_ _", __func__, event);
            if (this->is_starting()) { this->setstate(pipeline_state::started); }
            if (event.in()) {
                if (this->_buffer->is_safe_to_compact()) { this->_buffer->compact(); }
                this->_buffer->pubfill();
                this->_proto.receive_kernels(
                    this->_stream,
                    [this] (kernel_type* kernel) {
                        if (auto* app = dynamic_cast<Application_kernel*>(kernel)) {
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
        set_name(const char* rhs) noexcept {
            this->pipeline_base::set_name(rhs);
            this->_proto.set_name(rhs);
            #ifndef NDEBUG
            if (this->_buffer) {
                this->_buffer->set_name(rhs);
            }
            #endif
        }

    private:

        void
        receive_kernels(stream_type& stream) {
            while (stream.read_packet()) {
                Application_kernel* k = nullptr;
                try {
                    // eats remaining bytes on exception
                    kernel_header::application_ptr ptr = nullptr;
                    kernel_header hdr;
                    ipacket_guard g(stream.rdbuf());
                    kernel_type* tmp = nullptr;
                    stream >> hdr;
                    stream >> tmp;
                    k = dynamic_cast<Application_kernel*>(tmp);
                    #ifndef NDEBUG
                    this->log("recv _", *k);
                    #endif
                    application app(k->arguments(), k->environment());
                    sys::user_credentials creds = this->socket().credentials();
                    app.workdir(k->workdir());
                    app.set_credentials(creds.uid, creds.gid);
                    app.make_master();
                    try {
                        k->application(app.id());
                        router_type::execute(app);
                        k->return_to_parent(exit_code::success);
                    } catch (const sys::bad_call& err) {
                        k->return_to_parent(exit_code::error);
                        k->set_error(err.what());
                        this->log("execute error _,app=_", err, app.id());
                    } catch (const std::exception& err) {
                        k->return_to_parent(exit_code::error);
                        k->set_error(err.what());
                        this->log("execute error _,app=_", err.what(), app.id());
                    } catch (...) {
                        k->return_to_parent(exit_code::error);
                        k->set_error("unknown error");
                        this->log("execute error _", "<unknown>");
                    }
                } catch (const error& err) {
                    this->log("read error _", err);
                } catch (const std::exception& err) {
                    this->log("read error _ ", err.what());
                } catch (...) {
                    this->log("read error _", "<unknown>");
                }
                if (k) {
                    try {
                        opacket_guard g(stream);
                        stream.begin_packet();
                        kernel_header hdr;
                        hdr.setapp(this_application::get_id());
                        stream << hdr;
                        stream << k;
                        stream.end_packet();
                    } catch (const error& err) {
                        this->log("write error _", err);
                    } catch (const std::exception& err) {
                        this->log("write error _", err.what());
                    } catch (...) {
                        this->log("write error _", "<unknown>");
                    }
                    delete k;
                }
            }
        }

    };

    template<class K, class R>
    class unix_socket_pipeline: public basic_socket_pipeline<K> {

    public:
        using kernel_type = K;
        using router_type = R;
        using client_type = unix_socket_client<K,R>;
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
        void process_kernel(kernel_type* kernel);

        template <class X, class Y>
        friend class unix_socket_server;
        template <class X, class Y>
        friend class unix_socket_client;

    };

}

#endif // vim:filetype=cpp
