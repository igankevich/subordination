#ifndef FACTORY_NIC_SERVER_HH
#define FACTORY_NIC_SERVER_HH

#include <map>
#include <type_traits>

#include <stdx/algorithm.hh>
#include <stdx/iterator.hh>
#include <stdx/mutex.hh>

#include <sys/event.hh>
#include <sys/socket.hh>
#include <sys/fildesbuf.hh>
#include <sys/packetstream.hh>
#include <sys/endpoint.hh>
#include <sys/ifaddr.hh>

#include <factory/bits/server_category.hh>
#include <factory/proxy_server.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

namespace factory {

	template<class T, class Socket, class Router, class Kernels=std::deque<T*>>
	struct Remote_Rserver: public Server_base {

		typedef Server_base base_server;
		typedef T kernel_type;
		typedef char Ch;
		typedef basic_kernelbuf<sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>> Kernelbuf;
		typedef Kernel_stream<kernel_type> stream_type;
		typedef Server_base server_type;
		typedef Socket socket_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef application_type app_type;

		static_assert(
			std::is_move_constructible<stream_type>::value,
			"bad stream_type"
		);

		Remote_Rserver() = default;

		Remote_Rserver(socket_type&& sock, sys::endpoint vaddr, router_type& router):
		_vaddr(vaddr),
		_packetbuf(),
		_stream(&_packetbuf),
		_sentupstream(),
		_router(router)
		{
			_stream.setforward(
				[this] (app_type app) {
					Kernel_header hdr;
					hdr.from(_vaddr);
					hdr.setapp(app);
					_router.forward(hdr, _stream);
				}
			);
			_packetbuf.setfd(std::move(sock));
		}

		Remote_Rserver(const Remote_Rserver&) = delete;
		Remote_Rserver& operator=(const Remote_Rserver&) = delete;

		Remote_Rserver(Remote_Rserver&& rhs):
		base_server(std::move(rhs)),
		_vaddr(rhs._vaddr),
		_packetbuf(std::move(rhs._packetbuf)),
		_stream(std::move(rhs._stream)),
		_sentupstream(std::move(rhs._sentupstream)),
		_router(rhs._router)
		{
			_stream.rdbuf(&_packetbuf);
		}

		virtual
		~Remote_Rserver() {
			this->recover_kernels();
			this->delete_kernels();
		}

		void
		recover_kernels() {

			// Here failed kernels are written to buffer,
			// from which they must be recovered with recover_kernels().
			sys::poll_event ev{socket().fd(), sys::poll_event::In};
			handle(ev);

			// recover kernels from upstream and downstream buffer
			do_recover_kernels(_sentupstream);
			if (socket().error()) {
				do_recover_kernels(_sentdownstream);
			}
		}

		void
		send(kernel_type* kernel) {
			bool delete_kernel = false;
			if (kernel_goes_in_upstream_buffer(kernel)) {
				_sentupstream.push_back(kernel);
			} else
			if (kernel_goes_in_downstream_buffer(kernel)) {
				_sentdownstream.push_back(kernel);
			} else
			if (not kernel->moves_everywhere()) {
				delete_kernel = true;
			}
			#ifndef NDEBUG
			stdx::debug_message("nic") << "send " << stdx::make_object("to", vaddr(), "kernel", *kernel);
			#endif
			_stream << kernel;
			/// The kernel is deleted if it goes downstream
			/// and does not carry its parent.
			if (delete_kernel) {
				delete kernel;
			}
		}

		void
		handle(sys::poll_event& event) {
			_stream.clear();
			_stream.sync();
			kernel_type* kernel = nullptr;
			while (_stream >> kernel) {
				receive_kernel(kernel);
			}
		}

		void
		prepare(sys::poll_event& event) {
			if (_packetbuf.dirty()) {
				event.setev(sys::poll_event::Out);
			} else {
				event.unsetev(sys::poll_event::Out);
			}
		}

		const socket_type&
		socket() const {
			return _packetbuf.fd();
		}

		socket_type&
		socket() {
			return _packetbuf.fd();
		}

		void
		socket(sys::socket&& rhs) {
			_packetbuf.pubsync();
			_packetbuf.setfd(socket_type(std::move(rhs)));
		}

		const sys::endpoint& vaddr() const { return _vaddr; }
		void setvaddr(const sys::endpoint& rhs) { _vaddr = rhs; }

		friend std::ostream&
		operator<<(std::ostream& out, const Remote_Rserver& rhs) {
			return out << stdx::make_object(
				"vaddr", rhs.vaddr(),
				"socket", rhs.socket(),
				"kernels", rhs._sentupstream.size()
			);
		}

	private:

		void
		do_recover_kernels(pool_type& rhs) noexcept {
			using namespace std::placeholders;
			std::for_each(
				stdx::front_popper(rhs),
				stdx::front_popper_end(rhs),
				std::bind(&Remote_Rserver::recover_kernel, this, _1)
			);
		}

		void
		delete_kernels() {
			do_delete_kernels(_sentupstream);
			do_delete_kernels(_sentdownstream);
		}

		void
		do_delete_kernels(pool_type& rhs) noexcept {
			stdx::delete_each(
				stdx::front_popper(rhs),
				stdx::front_popper_end(rhs)
			);
		}

		static bool
		kernel_goes_in_upstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_upstream() or rhs->moves_somewhere();
		}

		static bool
		kernel_goes_in_downstream_buffer(const kernel_type* rhs) noexcept {
			return rhs->moves_downstream() and rhs->carries_parent();
		}

		void
		receive_kernel(kernel_type* k) {
			bool ok = true;
			k->from(_vaddr);
			if (k->moves_downstream()) {
				this->clear_kernel_buffer(k);
			} else if (k->principal_id()) {
				kernel_type* p = factory::instances.lookup(k->principal_id());
				if (p == nullptr) {
					k->result(Result::no_principal_found);
					ok = false;
				}
				k->principal(p);
			}
			#ifndef NDEBUG
			stdx::debug_message("nic", "recv") << *k;
			#endif
			if (!ok) {
				return_kernel(k);
			} else {
				_router.send_local(k);
			}
		}

		void return_kernel(kernel_type* k) {
			#ifndef NDEBUG
			stdx::debug_message("nic") << "No principal found for " << *k;
			#endif
			k->principal(k->parent());
			this->send(k);
		}

		void recover_kernel(kernel_type* k) {
			if (k->moves_upstream()) {
				#ifndef NDEBUG
				stdx::debug_message("nic") << "Recovering kernel " << *k;
				#endif
				_router.send_remote(k);
			} else if (k->moves_somewhere()) {
				#ifndef NDEBUG
				stdx::debug_message("nic") << "Destination is unreachable for " << *k;
				#endif
				k->from(k->to());
				k->result(Result::endpoint_not_connected);
				k->principal(k->parent());
				_router.send_local(k);
			} else if (k->moves_downstream() and k->carries_parent()) {
				#ifndef NDEBUG
				stdx::debug_message("nic") << "Reviving parent kernel on a backup node " << *k;
				#endif
				_router.send_local(k);
			} else {
				assert(false and "Bad kernel in sent buffer");
			}
		}

		void clear_kernel_buffer(kernel_type* k) {
			auto pos = std::find_if(
				_sentupstream.begin(),
				_sentupstream.end(),
				[k] (kernel_type* rhs) { return *rhs == *k; }
			);
			if (pos != _sentupstream.end()) {
				kernel_type* orig = *pos;
				k->parent(orig->parent());
				k->principal(k->parent());
				delete orig;
				_sentupstream.erase(pos);
			}
		}

		sys::endpoint _vaddr;
		Kernelbuf _packetbuf;
		stream_type _stream;
		pool_type _sentupstream;
		pool_type _sentdownstream;
		router_type& _router;
	};

	template<class T, class Socket, class Router>
	struct NIC_server: public Proxy_server<T,Remote_Rserver<T,Socket,Router>> {

		typedef Socket socket_type;
		typedef Router router_type;

		typedef Proxy_server<T,Remote_Rserver<T,Socket,Router>> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;
		using typename base_server::server_type;

		using base_server::poller;
		using base_server::send;

		typedef std::map<sys::endpoint,server_type> upstream_type;
		typedef typename upstream_type::iterator iterator_type;
		typedef server_type* handler_type;
		typedef sys::event_poller<handler_type> poller_type;
		typedef sys::ifaddr<sys::ipv4_addr> network_type;
		typedef network_type::rep_type rep_type;
		typedef Mobile_kernel::id_type id_type;

		static_assert(
			std::is_move_constructible<server_type>::value,
			"bad server_type"
		);

		static_assert(
			sizeof(rep_type) <= sizeof(id_type),
			"bad id_type"
		);

		NIC_server(NIC_server&& rhs) noexcept:
		base_server(std::move(rhs)),
		_upstream(),
		_iterator(_upstream.end()),
		_counter(rhs._counter),
		_router(rhs._router)
		{}

		NIC_server() = default;
		~NIC_server() = default;
		NIC_server(const NIC_server&) = delete;
		NIC_server& operator=(const NIC_server&) = delete;

		void
		remove_server(server_type* ptr) override {
			// TODO: occasional ``Bad file descriptor''
			#ifndef NDEBUG
			stdx::debug_message("nic") << "remove " << *ptr;
			#endif
			auto result = _upstream.find(ptr->vaddr());
			if (result != _upstream.end()) {
				remove_valid_server(result);
			}
		}

		void accept_connection(sys::poll_event&) override {
			sys::endpoint addr;
			socket_type sock;
			_socket.accept(sock, addr);
			sys::endpoint vaddr = virtual_addr(addr);
			auto res = _upstream.find(vaddr);
			if (res == _upstream.end()) {
				server_type* ptr = add_connected_server(std::move(sock), vaddr, sys::poll_event::In);
				#ifndef NDEBUG
				stdx::debug_message("nic") << "accept " << *ptr;
				#endif
			} else {
				server_type& s = res->second;
				const sys::port_type local_port = s.socket().bind_addr().port();
				if (!(addr.port() < local_port)) {
					#ifndef NDEBUG
					stdx::debug_message("nic") << "replace " << s;
					#endif
					poller().disable(s.socket().fd());
					server_type new_s(std::move(s));
					// new_s.setparent(this);
					new_s.socket(std::move(sock));
					remove_valid_server(res);
//						_upstream.erase(res);
					_upstream.emplace(vaddr, std::move(new_s));
//						_upstream.emplace(vaddr, std::move(*new_s));
					poller().emplace(
						sys::poll_event{res->second.socket().fd(), sys::poll_event::Inout, sys::poll_event::Inout},
						handler_type(&res->second));
				}
			}
		}

		void peer(sys::endpoint addr) {
			lock_type lock(this->_mutex);
			this->connect_to_server(addr, sys::poll_event::In);
		}

		void
		bind(const sys::endpoint& rhs, sys::ipv4_addr netmask) {
			_network = network_type(rhs.addr4(), netmask);
			_counter = determine_initial_id();
			socket(rhs);
		}

		void
		socket(const sys::endpoint& addr) {
			_socket.bind(addr);
			_socket.listen();
			poller().insert_special(sys::poll_event{_socket.fd(),
				sys::poll_event::In});
			if (not this->is_stopped()) {
				this->_semaphore.notify_one();
			}
		}

		inline sys::endpoint
		server_addr() const {
			return _socket.bind_addr();
		}

	private:

		id_type
		generate_id() noexcept {
			return _counter++;
		}

		id_type
		determine_initial_id() noexcept {
			const id_type x0 = _network.begin()->position(_network.netmask());
			const id_type x1 = _network.end()->position(_network.netmask());
			const id_type min_id = std::numeric_limits<id_type>::min();
			const id_type max_id = std::numeric_limits<id_type>::max();
			const id_type chunk_size = (max_id-min_id) / (x1-x0);
			id_type initial_id = (_network.position()-x0+1) * chunk_size;
			if (initial_id == 0) ++initial_id;
			return initial_id;
		}

		void
		ensure_identity_helper(kernel_type* kernel) {
			if (not kernel->identifiable()) {
				kernel->set_id(generate_id());
			}
		}

		void
		ensure_identity(kernel_type* kernel) {
			ensure_identity_helper(kernel);
			ensure_identity_helper(kernel->parent());
		}

		void
		remove_valid_server(iterator_type result) noexcept {
			if (result == _iterator) {
				advance_upstream_iterator();
			}
			result->second.setstate(server_state::stopped);
			_upstream.erase(result);
		}

		void
		advance_upstream_iterator() noexcept {
			_endreached = (++_iterator == _upstream.end());
			if (_endreached) {
				_iterator = _upstream.begin();
			}
		}

		std::pair<iterator_type,bool>
		emplace_server(const sys::endpoint& vaddr, server_type&& s) {
			auto result = _upstream.emplace(vaddr, std::move(s));
			if (_upstream.size() == 1) {
				_iterator = _upstream.begin();
			}
			return result;
		}

		inline sys::endpoint
		virtual_addr(sys::endpoint addr) const {
			return sys::endpoint(addr, server_addr().port());
		}

		void
		process_kernels() override {
			stdx::front_pop_iterator<kernel_pool> it_end;
			lock_type lock(this->_mutex);
			stdx::for_each_thread_safe(lock,
				stdx::front_popper(this->_kernels), it_end,
				[this] (kernel_type* rhs) { process_kernel(rhs); }
			);
		}

		void
		process_kernel(kernel_type* k) {
			if (this->server_addr() && k->to() == this->server_addr()) {
				_router.send_local(k);
//					std::ostringstream msg;
//					msg << "Kernel is sent to local node. From="
//						<< this->server_addr() << ", to=" << k->to();
//					throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}

			if (k->moves_everywhere()) {
				for (auto& pair : _upstream) {
					pair.second.send(k);
				}
				// delete broadcast kernel
				delete k;
			} else if (k->moves_upstream() && k->to() == sys::endpoint()) {
				if (_upstream.empty() or _endreached) {
					// include localhost in round-robin
					// in a somewhat half-assed fashion
					_endreached = false;
					// short-circuit kernels when no upstream servers are available
					_router.send_local(k);
				} else {
					// skip stopped hosts
					iterator_type old_iterator = _iterator;
					if (_iterator->second.is_stopped()) {
						do {
							advance_upstream_iterator();
						} while (_iterator->second.is_stopped() and old_iterator != _iterator);
					}
					// round robin over upstream hosts
					ensure_identity(k);
					_iterator->second.send(k);
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
					ensure_identity(k);
				}
				this->find_or_create_peer(k->to(), sys::poll_event::Inout)->send(k);
			}
		}

		server_type* find_or_create_peer(const sys::endpoint& addr, sys::poll_event::legacy_event ev) {
			server_type* ret;
			auto result = _upstream.find(addr);
			if (result == _upstream.end()) {
				ret = this->connect_to_server(addr, ev);
			} else {
				ret = &result->second;
			}
			return ret;
		}

		server_type* connect_to_server(sys::endpoint addr, sys::poll_event::legacy_event events) {
			// bind to server address with ephemeral port
			sys::endpoint srv_addr(this->server_addr(), 0);
			return this->add_connected_server(socket_type(srv_addr, addr), addr, events);
		}

		server_type* add_connected_server(socket_type&& sock, sys::endpoint vaddr,
			sys::poll_event::legacy_event events,
			sys::poll_event::legacy_event revents=0)
		{
			sys::fd_type fd = sock.fd();
			server_type s(std::move(sock), vaddr, _router);
			// s.setparent(this);
			auto result = emplace_server(vaddr, std::move(s));
			poller().emplace(
				sys::poll_event{fd, events, revents},
				handler_type(&result.first->second));
			#ifndef NDEBUG
			stdx::debug_message("nic") << "add " << result.first->second;
			#endif
			return &result.first->second;
		}

		network_type _network;
		socket_type _socket;
		upstream_type _upstream;
		iterator_type _iterator;
		bool _endreached = false;
		std::atomic<id_type> _counter{0};
		router_type _router;
	};

}

#endif // FACTORY_NIC_SERVER_HH
