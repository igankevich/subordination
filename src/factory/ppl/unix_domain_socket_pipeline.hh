#ifndef FACTORY_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH
#define FACTORY_PPL_UNIX_DOMAIN_SOCKET_PIPELINE_HH

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <unistdx/net/endpoint>
#include <unistdx/net/socket>

#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/external_process_handler.hh>

namespace factory {

	template<class K, class R>
	class unix_domain_socket_pipeline:
		public basic_socket_pipeline<K,external_process_handler<K,R>> {

	private:
		typedef basic_socket_pipeline<K,external_process_handler<K,R>>
		    base_pipeline;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::queue_popper;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::event_handler_type;
		typedef typename sem_type::const_iterator client_iterator;
		typedef event_handler_ptr client_type;
		typedef std::pair<sys::endpoint, sys::socket> server_type;
		typedef std::vector<server_type> server_container;
		typedef server_container::iterator server_iterator;

	public:
		typedef K kernel_type;
		typedef R router_type;

	private:
		server_container _servers;

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

	protected:

		void
		remove_server(sys::fd_type fd) override;

		void
		remove_client(event_handler_ptr ptr) override;

		void
		accept_connection(sys::poll_event& ev) override;

		void
		process_kernels() override;

	private:

		void
		process_kernel(kernel_type* k);

		server_iterator
		find_server(const sys::endpoint& e);

		server_iterator
		find_server(sys::fd_type fd);

		client_iterator
		find_client(const sys::endpoint& e);

		void
		add_client(const sys::endpoint& addr, sys::socket&& sock);

	};

}

#endif // vim:filetype=cpp
