#ifndef BSCHEDULER_PPL_SOCKET_PIPELINE_HH
#define BSCHEDULER_PPL_SOCKET_PIPELINE_HH

#include <iosfwd>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/it/field_iterator>
#include <unistdx/net/socket_address>
#include <unistdx/net/interface_address>

#include <bscheduler/kernel/kernel_instance_registry.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/basic_socket_pipeline.hh>
#include <bscheduler/ppl/local_server.hh>

namespace bsc {

	template <class K, class S, class R>
	class local_server;

	template <class K, class S, class R>
	class remote_client;

	template <class K, class S, class R>
	class socket_notify_handler;

	template<class K, class S, class R>
	class socket_pipeline: public basic_socket_pipeline<K> {

	public:
		typedef S socket_type;
		typedef R router_type;
		typedef sys::ipv4_address addr_type;
		typedef sys::interface_address<addr_type> ifaddr_type;
		typedef remote_client<K,S,R> remote_client_type;
		typedef local_server<K,S,R> server_type;
		typedef std::shared_ptr<server_type> server_ptr;
		typedef basic_socket_pipeline<K> base_pipeline;

		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::duration;

	private:
		typedef remote_client_type event_handler_type;
		typedef std::shared_ptr<event_handler_type> event_handler_ptr;
		typedef sys::ipaddr_traits<addr_type> traits_type;
		typedef std::vector<server_ptr> server_container_type;
		typedef typename server_container_type::iterator server_iterator;
		typedef typename server_container_type::const_iterator
		    server_const_iterator;
		typedef std::unordered_map<sys::socket_address,event_handler_ptr>
		    client_container_type;
		typedef typename client_container_type::iterator client_iterator;
		typedef ifaddr_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;
		typedef sys::field_iterator<server_const_iterator,0> ifaddr_iterator;
		typedef event_handler_type client_type;
		typedef event_handler_ptr client_ptr;
		typedef uint32_t weight_type;

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

		socket_pipeline();

		~socket_pipeline() = default;

		socket_pipeline(const socket_pipeline&) = delete;

		socket_pipeline(socket_pipeline&&) = delete;

		socket_pipeline&
		operator=(const socket_pipeline&) = delete;

		socket_pipeline&
		operator=(socket_pipeline&&) = delete;

		void
		add_client(const sys::socket_address& addr) {
			lock_type lock(this->_mutex);
			this->do_add_client(addr);
		}

		void
		stop_client(const sys::socket_address& addr);

		void
		set_client_weight(const sys::socket_address& addr, weight_type new_weight);

		void
		add_server(const ifaddr_type& rhs) {
			this->add_server(
				sys::socket_address(rhs.address(), this->_port),
				rhs.netmask()
			);
		}

		void
		add_server(const sys::socket_address& rhs, addr_type netmask);

		void
		forward(foreign_kernel* hdr);

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

		void
		remove_server(const ifaddr_type& interface_address);

		void
		print_state(std::ostream& out);

	private:

		void
		remove_client(const sys::socket_address& vaddr);

		void
		remove_client(client_iterator result);

		void
		remove_server(server_iterator result);

		server_iterator
		find_server(const ifaddr_type& interface_address);

		server_iterator
		find_server(sys::fd_type fd);

		server_iterator
		find_server(const sys::socket_address& dest);

		void
		ensure_identity(kernel_type* k, const sys::socket_address& dest);

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

		void
		emplace_client(const sys::socket_address& vaddr, const event_handler_ptr& s);

		inline sys::socket_address
		virtual_addr(const sys::socket_address& addr) const {
			return addr.family() == sys::family_type::unix
			       ? addr
				   : sys::socket_address(addr, this->_port);
		}

		void
		process_kernels() override;

		void
		process_kernel(kernel_type* k);

		event_handler_ptr
		find_or_create_client(const sys::socket_address& addr);

		event_handler_ptr
		do_add_client(const sys::socket_address& addr);

		event_handler_ptr
		do_add_client(socket_type&& sock, sys::socket_address vaddr);

		template <class K1, class S1, class R1>
		friend class local_server;

		template <class K1, class S1, class R1>
		friend class remote_client;

		template <class K1, class S1, class R1>
		friend class socket_notify_handler;

	};

}

#endif // vim:filetype=cpp
