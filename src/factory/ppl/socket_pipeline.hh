#ifndef FACTORY_PPL_SOCKET_PIPELINE_HH
#define FACTORY_PPL_SOCKET_PIPELINE_HH

#include <tuple>
#include <unordered_map>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/it/field_iterator>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/local_server.hh>
#include <factory/ppl/remote_client.hh>

namespace factory {

	template<class T, class Socket, class Router>
	struct socket_pipeline:
		public basic_socket_pipeline<T,remote_client<T,Socket,Router>> {

		typedef Socket socket_type;
		typedef Router router_type;
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef remote_client<T,Socket,Router> remote_client_type;
		typedef local_server<addr_type,socket_type> server_type;
		typedef basic_socket_pipeline<T,remote_client_type> base_pipeline;

		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::duration;

		using base_pipeline::poller;
		using base_pipeline::send;

	private:
		typedef sys::ipaddr_traits<addr_type> traits_type;
		typedef std::vector<server_type> server_container_type;
		typedef typename server_container_type::iterator server_iterator;
		typedef typename server_container_type::const_iterator
			server_const_iterator;
		typedef std::unordered_map<sys::endpoint,event_handler_ptr>
			client_map_type;
		typedef typename client_map_type::iterator client_iterator;
		typedef ifaddr_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;
		typedef sys::field_iterator<server_const_iterator,0> ifaddr_iterator;

	private:
		server_container_type _servers;
		client_map_type _clients;
		client_iterator _iterator;
		bool _endreached = false;
		router_type _router;
		sys::port_type _port = 33333;
		std::chrono::milliseconds _socket_timeout = std::chrono::seconds(7);

	public:

		socket_pipeline(socket_pipeline&& rhs):
		base_pipeline(std::move(rhs)),
		_servers(std::move(rhs._servers)),
		_clients(),
		_iterator(_clients.end()),
		_router(rhs._router),
		_port(rhs._port)
		{
		}

		socket_pipeline() {
			using namespace std::chrono;
			this->set_start_timeout(seconds(7));
		}

		~socket_pipeline() = default;
		socket_pipeline(const socket_pipeline&) = delete;
		socket_pipeline& operator=(const socket_pipeline&) = delete;
		socket_pipeline& operator=(socket_pipeline&&) = delete;

		void
		remove_server(sys::fd_type fd) override {
			server_iterator result = this->find_server(fd);
			if (result != this->_servers.end()) {
				this->remove_server(result);
			}
		}

		void
		remove_client(event_handler_ptr ptr) override {
			client_iterator result = _clients.find(ptr->vaddr());
			if (result != this->_clients.end()) {
				this->remove_client(result);
			}
		}

		void accept_connection(sys::poll_event& ev) override {
			sys::endpoint addr;
			socket_type sock;
			server_iterator result = this->find_server(ev.fd());
			if (result == this->_servers.end()) {
				throw std::invalid_argument("no matching server found");
			}
			result->socket().accept(sock, addr);
			sys::endpoint vaddr = virtual_addr(addr);
			auto res = _clients.find(vaddr);
			if (res == _clients.end()) {
				#ifndef NDEBUG
				event_handler_ptr ptr =
				#endif
				add_connected_pipeline(std::move(sock), vaddr, sys::poll_event::In);
				#ifndef NDEBUG
				sys::log_message(this->_name, "accept _", *ptr);
				#endif
			} else {
				/*
				event_handler_ptr s = res->second;
				const sys::port_type local_port = s->socket().bind_addr().port();
				if (!(addr.port() < local_port)) {
					#ifndef NDEBUG
					sys::log_message(this->_name,  "replace _", s);
					#endif
					poller().disable(s->socket().fd());
					s->socket(std::move(sock));
					remove_client(res);
					_clients.emplace(vaddr, std::move(s));
					poller().emplace(sys::poll_event{s->socket().fd(), sys::poll_event::Inout, sys::poll_event::Inout}, s);
				}
				*/
			}
		}

		void
		add_client(const sys::endpoint& addr) {
			lock_type lock(this->_mutex);
			this->add_client(addr, sys::poll_event::In);
		}

		void
		add_server(const ifaddr_type& rhs) {
			this->add_server(
				sys::endpoint(rhs.address(), this->_port),
				rhs.netmask()
			);
		}

		void
		add_server(const sys::endpoint& rhs, addr_type netmask) {
			lock_type lock(this->_mutex);
			ifaddr_type ifaddr(traits_type::address(rhs), netmask);
			if (this->find_server(ifaddr) == this->_servers.end()) {
				this->_servers.emplace_back(ifaddr, traits_type::port(rhs));
				server_type& srv = this->_servers.back();
				srv.socket().set_user_timeout(this->_socket_timeout);
				sys::fd_type fd = srv.socket().get_fd();
				this->poller().insert_special(
					sys::poll_event{fd, sys::poll_event::In}
				);
				if (!this->is_stopped()) {
					this->_semaphore.notify_one();
				}
			}
		}

		void
		remove_server(const ifaddr_type& ifaddr) {
			lock_type lock(this->_mutex);
			server_iterator result = this->find_server(ifaddr);
			if (result != this->_servers.end()) {
				this->remove_server(result);
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			lock_type lock(this->_mutex);
			this->log("fwd _", hdr);
			// TODO
		}

		inline void
		set_port(sys::port_type rhs) noexcept {
			this->_port = rhs;
		}

		inline server_const_iterator
		servers_begin() const noexcept {
			return this->_servers.begin();
		}

		inline server_const_iterator
		servers_end() const noexcept {
			return this->_servers.end();
		}

	private:

		void
		remove_client(client_iterator result) {
			#ifndef NDEBUG
			const char* reason =
				result->second->is_starting() ? "timed out" : "";
			sys::log_message(
				this->_name,
				"remove client _ (_)",
				result->first,
				reason
			);
			#endif
			if (result == this->_iterator) {
				this->advance_upstream_iterator();
			}
			result->second->setstate(pipeline_state::stopped);
			this->_clients.erase(result);
		}

		void
		remove_server(server_iterator result) {
			#ifndef NDEBUG
			sys::log_message(this->_name, "remove server _", result->ifaddr());
			#endif
			this->_servers.erase(result);
		}

		server_iterator
		find_server(const ifaddr_type& ifaddr) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&ifaddr] (const value_type& rhs) {
					return rhs.ifaddr() == ifaddr;
				}
			);
		}

		server_iterator
		find_server(sys::fd_type fd) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[fd] (const value_type& rhs) {
					return rhs.socket().get_fd() == fd;
				}
			);
		}

		server_iterator
		find_server(const sys::endpoint& dest) {
			typedef typename server_container_type::value_type value_type;
			return std::find_if(
				this->_servers.begin(),
				this->_servers.end(),
				[&dest] (const value_type& rhs) {
					return rhs.ifaddr().contains(dest.addr4());
				}
			);
		}

		void
		set_kernel_id(kernel_type* kernel, server_type& srv) {
			if (!kernel->has_id()) {
				kernel->set_id(srv.generate_id());
			}
		}

		void
		ensure_identity(kernel_type* kernel, const sys::endpoint& dest) {
			if (this->_servers.empty()) {
				kernel->return_to_parent(
					exit_code::no_upstream_servers_available
				);
			} else {
				server_iterator result = this->find_server(dest);
				if (result == this->_servers.end()) {
					result = this->_servers.begin();
				}
				this->set_kernel_id(kernel, *result);
				this->set_kernel_id(kernel->parent(), *result);
			}
		}

		void
		advance_upstream_iterator() noexcept {
			_endreached = (++_iterator == _clients.end());
			if (_endreached) {
				_iterator = _clients.begin();
			}
		}

		std::pair<client_iterator,bool>
		emplace_pipeline(const sys::endpoint& vaddr, event_handler_ptr&& s) {
			auto result = _clients.emplace(vaddr, std::move(s));
			if (_clients.size() == 1) {
				_iterator = _clients.begin();
			}
			return result;
		}

		inline sys::endpoint
		virtual_addr(sys::endpoint addr) const {
			return sys::endpoint(addr, this->_port);
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			sys::for_each_unlock(lock,
				sys::queue_popper(this->_kernels),
				sys::queue_popper_end(this->_kernels),
				[this] (kernel_type* rhs) { process_kernel(rhs); }
			);
		}

		void
		process_kernel(kernel_type* k) {
			// short circuit local server
			if (k->to()) {
				server_iterator result = this->find_server(k->to());
				if (result != this->_servers.end()) {
					this->_router.send_local(k);
				}
			}
			if (k->moves_everywhere()) {
				for (auto& pair : _clients) {
					pair.second->send(k);
				}
				// delete broadcast kernel
				delete k;
			} else if (k->moves_upstream() && k->to() == sys::endpoint()) {
				if (_clients.empty() or _endreached) {
					// include localhost in round-robin
					// in a somewhat half-assed fashion
					_endreached = false;
					// short-circuit kernels when no upstream servers are available
					_router.send_local(k);
				} else {
					// skip stopped hosts
					client_iterator old_iterator = _iterator;
					if (_iterator->second->is_stopped()) {
						do {
							advance_upstream_iterator();
						} while (_iterator->second->is_stopped() and old_iterator != _iterator);
					}
					// round robin over upstream hosts
					ensure_identity(k, this->_iterator->second->vaddr());
					_iterator->second->send(k);
					advance_upstream_iterator();
				}
			} else if (k->moves_downstream() and not k->from()) {
				// kernel @k was sent to local node
				// because no upstream servers had
				// been available
				_router.send_local(k);
			} else {
				// create endpoint if necessary, and send kernel
				if (not k->to()) {
					k->to(k->from());
				}
				if (k->moves_somewhere()) {
					ensure_identity(k, k->to());
				}
				this->find_or_create_peer(k->to(), sys::poll_event::Inout)->send(k);
			}
		}

		event_handler_ptr
		find_or_create_peer(const sys::endpoint& addr, sys::poll_event::legacy_event ev) {
			event_handler_ptr ret;
			auto result = _clients.find(addr);
			if (result == _clients.end()) {
				ret = this->add_client(addr, ev);
			} else {
				ret = result->second;
			}
			return ret;
		}

		event_handler_ptr
		add_client(sys::endpoint addr, sys::poll_event::legacy_event events) {
			server_iterator result = this->find_server(addr);
			if (result == this->_servers.end()) {
				throw std::invalid_argument("no matching server found");
			}
			// bind to server address with ephemeral port
			sys::endpoint srv_addr(result->endpoint(), 0);
			return this->add_connected_pipeline(socket_type(srv_addr, addr), addr, events);
		}

		event_handler_ptr
		add_connected_pipeline(socket_type&& sock, sys::endpoint vaddr,
			sys::poll_event::legacy_event events,
			sys::poll_event::legacy_event revents=0)
		{
			sys::fd_type fd = sock.fd();
			sock.set_user_timeout(this->_socket_timeout);
			event_handler_ptr s =
				std::make_shared<event_handler_type>(
					std::move(sock),
					vaddr,
					_router
				);
			s->setstate(pipeline_state::starting);
			// s.setparent(this);
			auto result = emplace_pipeline(vaddr, std::move(s));
			this->poller().emplace(
				sys::poll_event{fd, events, revents},
				result.first->second
			);
			#ifndef NDEBUG
			sys::log_message(this->_name, "add _", *result.first->second);
			#endif
			if (!this->is_stopped()) {
				this->_semaphore.notify_one();
			}
			return result.first->second;
		}

	};

}

#endif // vim:filetype=cpp
