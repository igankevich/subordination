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

	template<class T, class Router>
	class unix_domain_socket_pipeline:
		public basic_socket_pipeline<T,external_process_handler<T,Router>> {

	public:
		typedef basic_socket_pipeline<T,external_process_handler<T,Router>>
			base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		typedef Router router_type;

		using base_pipeline::poller;

	private:
		typedef std::pair<sys::endpoint, sys::socket> server_type;
		typedef std::vector<server_type> server_container;
		typedef server_container::iterator server_iterator;
		typedef event_handler_ptr client_type;
		typedef typename sem_type::const_iterator client_iterator;

	private:
		server_container _servers;

	public:
		unix_domain_socket_pipeline() = default;
		~unix_domain_socket_pipeline() = default;
		unix_domain_socket_pipeline(const unix_domain_socket_pipeline& rhs) = delete;
		unix_domain_socket_pipeline(unix_domain_socket_pipeline&& rhs) = delete;

		void
		add_server(const sys::endpoint& rhs) {
			lock_type lock(this->_mutex);
			if (this->find_server(rhs) == this->_servers.end()) {
				sys::socket sock;
				sock.bind(rhs);
				sock.setopt(sys::socket::pass_credentials);
				sock.listen();
				this->_servers.emplace_back(rhs, std::move(sock));
				server_type& srv = this->_servers.back();
				sys::fd_type fd = srv.second.get_fd();
				this->poller().insert_special(
					sys::poll_event{fd, sys::poll_event::In}
				);
				if (!this->is_stopped()) {
					this->_semaphore.notify_one();
				}
			}
		}

		void
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			sys::socket s(sys::family_type::unix);
			s.setopt(sys::socket::pass_credentials);
			s.connect(addr);
			this->add_client(addr, std::move(s));
			this->poller().notify_one();
		}

	protected:

		void
		remove_server(sys::fd_type fd) override {
			server_iterator result = this->find_server(fd);
			if (result != this->_servers.end()) {
				#ifndef NDEBUG
				this->log("remove server _", result->first);
				#endif
				this->_servers.erase(result);
			}
		}

		void
		remove_client(event_handler_ptr ptr) override {
			#ifndef NDEBUG
			this->log("remove _ _", ptr->endpoint(), ptr->socket());
			#endif
		}

		void
		accept_connection(sys::poll_event& ev) override {
			sys::endpoint addr;
			sys::socket sock;
			server_iterator result = this->find_server(ev.fd());
			if (result == this->_servers.end()) {
				throw std::invalid_argument("no matching server found");
			}
			result->second.accept(sock, addr);
			client_iterator res = this->find_client(addr);
			if (res != this->poller().end()) {
				throw std::invalid_argument("client already exists");
			}
			this->add_client(addr, std::move(sock));
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

	private:

		void
		process_kernel(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
		}

		server_iterator
		find_server(const sys::endpoint& e) {
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&e] (const server_type& rhs) {
					return rhs.first == e;
				}
			);
		}

		server_iterator
		find_server(sys::fd_type fd) {
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[fd] (const server_type& rhs) {
					return rhs.second.get_fd() == fd;
				}
			);
		}

		client_iterator
		find_client(const sys::endpoint& e) {
			return std::find_if(
				this->poller().begin(),
				this->poller().end(),
				[&e] (const client_type& rhs) {
					return rhs->endpoint() == e;
				}
			);
		}

		void
		add_client(const sys::endpoint& addr, sys::socket&& sock) {
			sys::fd_type fd = sock.fd();
			event_handler_ptr s =
				std::make_shared<event_handler_type>(
					addr,
					std::move(sock)
				);
			s->setstate(pipeline_state::starting);
			this->poller().emplace(
				sys::poll_event{fd, sys::poll_event::In, sys::poll_event::In},
				s
			);
			#ifndef NDEBUG
			this->log("add _", addr);
			#endif
		}

	};

}

#endif // vim:filetype=cpp
