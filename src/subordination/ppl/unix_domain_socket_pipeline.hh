#ifndef SUBORDINATION_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH
#define SUBORDINATION_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <unistdx/net/socket_address>
#include <unistdx/net/socket>

#include <subordination/ppl/basic_socket_pipeline.hh>

namespace sbn {

    template<class K, class R>
    class unix_domain_socket_pipeline: public basic_socket_pipeline<K> {

    public:
        typedef K kernel_type;
        typedef R router_type;

    public:
        unix_domain_socket_pipeline() = default;

        ~unix_domain_socket_pipeline() = default;

        unix_domain_socket_pipeline(const unix_domain_socket_pipeline& rhs) =
            delete;

        unix_domain_socket_pipeline(unix_domain_socket_pipeline&& rhs) = delete;

        void
        add_server(const sys::socket_address& rhs);

        void
        add_client(const sys::socket_address& addr);

    private:

        void
        add_client(const sys::socket_address& addr, sys::socket&& sock);

        void
        process_kernels() override {}

        template <class X, class Y>
        friend class unix_socket_server;
        template <class X, class Y>
        friend class unix_socket_client;

    };

}

#endif // vim:filetype=cpp
