#ifndef BSCHEDULER_PPL_SOCKET_PIPELINE_HH
#define BSCHEDULER_PPL_SOCKET_PIPELINE_HH

#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/it/field_iterator>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

#include <bscheduler/kernel/kernel_instance_registry.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/basic_socket_pipeline.hh>
#include <bscheduler/ppl/local_server.hh>
#include <bscheduler/ppl/remote_client.hh>

namespace bsc {

	template<class T, class Socket, class Router>
	class socket_pipeline: public basic_socket_pipeline<T> {

	public:
		typedef Socket socket_type;
		typedef Router router_type;
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef remote_client<T,Socket,Router> remote_client_type;
		typedef local_server<addr_type,socket_type> server_type;
		typedef basic_socket_pipeline<T> base_pipeline;

		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::duration;

	private:
		typedef sys::ipaddr_traits<addr_type> traits_type;
		typedef std::vector<server_type> server_container_type;
		typedef typename server_container_type::iterator server_iterator;
		typedef typename server_container_type::const_iterator
			server_const_iterator;
		typedef std::unordered_map<sys::endpoint,event_handler_ptr>
			client_container_type;
		typedef typename client_container_type::iterator client_iterator;
		typedef ifaddr_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;
		typedef sys::field_iterator<server_const_iterator,0> ifaddr_iterator;
		typedef event_handler_type client_type;
		typedef event_handler_ptr client_ptr;
		typedef typename client_type::weight_type weight_type;

	private:
		server_container_type _servers;
		client_container_type _clients;
		/// Iterator to client container which is used to distribute the
		/// kernels between several clients taking into account their weight.
		client_iterator _iterator = this->_clients.end();
		/// Client weight counter. Goes from nought to the number of nodes
		/// "behind" the client.
		weight_type _weightcnt = 0;
		sys::port_type _port = 33333;
		std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);
		id_type _counter = 0;
		bool _uselocalhost = true;

	public:

		socket_pipeline() {
			using namespace std::chrono;
			this->set_start_timeout(seconds(7));
		}

		~socket_pipeline() = default;
		socket_pipeline(const socket_pipeline&) = delete;
		socket_pipeline(socket_pipeline&&) = delete;
		socket_pipeline& operator=(const socket_pipeline&) = delete;
		socket_pipeline& operator=(socket_pipeline&&) = delete;

		void
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			this->add_client(addr, sys::epoll_event::In);
		}

		void
		stop_client(const sys::endpoint& addr);

		void
		set_client_weight(const sys::endpoint& addr, weight_type new_weight);

		void
		add_server(const ifaddr_type& rhs) {
			this->add_server(
				sys::endpoint(rhs.address(), this->_port),
				rhs.netmask()
			);
		}

		void
		add_server(const sys::endpoint& rhs, addr_type netmask);

		void
		remove_server(const ifaddr_type& ifaddr);

		void
		forward(kernel_header& hdr, sys::pstream& istr);

		inline void
		set_port(sys::port_type rhs) noexcept {
			this->_port = rhs;
		}

		inline sys::port_type
		port() const noexcept {
			return this->_port;
		}

		inline server_const_iterator
		servers_begin() const noexcept {
			return this->_servers.begin();
		}

		inline server_const_iterator
		servers_end() const noexcept {
			return this->_servers.end();
		}

		inline void
		use_localhost(bool b) noexcept {
			this->_uselocalhost = b;
		}

	private:

		void
		remove_server(sys::fd_type fd) override;

		void
		remove_client(event_handler_ptr ptr) override;

		void
		accept_connection(sys::epoll_event& ev) override;

		void
		remove_client(client_iterator result);

		void
		remove_server(server_iterator result);

		server_iterator
		find_server(const ifaddr_type& ifaddr);

		server_iterator
		find_server(sys::fd_type fd);

		server_iterator
		find_server(const sys::endpoint& dest);

		void
		ensure_identity(kernel_type* k, const sys::endpoint& dest);

		/// round robin over upstream hosts
		void
		find_next_client();

		inline bool
		end_reached() const noexcept {
			return this->_iterator == this->_clients.end();
		}

		inline void
		reset_iterator() noexcept {
			this->_iterator = this->_clients.end();
			this->_weightcnt = 0;
		}

		inline const client_type&
		current_client() const noexcept {
			return *this->_iterator->second;
		}

		inline client_type&
		current_client() noexcept {
			return *this->_iterator->second;
		}

		inline void
		advance_client_iterator() noexcept {
			++this->_iterator;
			this->_weightcnt = 0;
		}

		std::pair<client_iterator,bool>
		emplace_pipeline(const sys::endpoint& vaddr, event_handler_ptr&& s);

		inline sys::endpoint
		virtual_addr(const sys::endpoint& addr) const {
			return addr.family() == sys::family_type::unix
				? addr
				: sys::endpoint(addr, this->_port);
		}

		void
		process_kernels() override;

		void
		process_kernel(kernel_type* k);

		event_handler_ptr
		find_or_create_peer(
			const sys::endpoint& addr,
			sys::epoll_event::legacy_event ev
		);

		event_handler_ptr
		add_client(
			const sys::endpoint& addr,
			sys::epoll_event::legacy_event events
		);

		event_handler_ptr
		add_connected_pipeline(
			socket_type&& sock,
			sys::endpoint vaddr,
			sys::epoll_event::legacy_event events,
			sys::epoll_event::legacy_event revents=0
		);

	};

}

#endif // vim:filetype=cpp
