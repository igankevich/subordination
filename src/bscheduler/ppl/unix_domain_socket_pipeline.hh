#ifndef BSCHEDULER_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH
#define BSCHEDULER_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <unistdx/net/endpoint>
#include <unistdx/net/socket>

#include <bscheduler/ppl/basic_socket_pipeline.hh>

namespace bsc {

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
		add_server(const sys::endpoint& rhs);

		void
		add_client(const sys::endpoint& addr);

	private:

		void
		add_client(const sys::endpoint& addr, sys::socket&& sock);

		template <class X, class Y> friend class unix_socket_server;
		template <class X, class Y> friend class unix_socket_client;

	};

}

#endif // vim:filetype=cpp
