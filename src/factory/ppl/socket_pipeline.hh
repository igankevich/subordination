#ifndef FACTORY_PPL_SOCKET_PIPELINE_HH
#define FACTORY_PPL_SOCKET_PIPELINE_HH

#include <map>
#include <type_traits>

#include <unistdx/base/delete_each>
#include <unistdx/base/unlock_guard>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/it/queue_popper>
#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>
#include <unistdx/net/pstream>
#include <unistdx/net/socket>

#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/basic_socket_pipeline.hh>

namespace factory {

	template<class T, class Socket, class Router, class Kernels=std::deque<T*>,
		class Traits=sys::deque_traits<Kernels>>
	struct remote_client: public pipeline_base {

		typedef pipeline_base base_pipeline;
		typedef T kernel_type;
		typedef char Ch;
		typedef basic_kernelbuf<sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>> Kernelbuf;
		typedef kstream<kernel_type> stream_type;
		typedef Socket socket_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef sys::pid_type app_type;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<Kernels,Traits> queue_popper;

		static_assert(
			std::is_move_constructible<stream_type>::value,
			"bad stream_type"
		);

		remote_client() = default;

		remote_client(socket_type&& sock, sys::endpoint vaddr, router_type& router):
		_vaddr(vaddr),
		_packetbuf(),
		_stream(&_packetbuf),
		_sentupstream(),
		_router(router)
		{
			_stream.setforward(
				[this] (app_type app) {
					kernel_header hdr;
					hdr.from(_vaddr);
					hdr.setapp(app);
					_router.forward(hdr, _stream);
				}
			);
			_packetbuf.setfd(std::move(sock));
		}

		remote_client(const remote_client&) = delete;
		remote_client& operator=(const remote_client&) = delete;

		remote_client(remote_client&& rhs):
		base_pipeline(std::move(rhs)),
		_vaddr(rhs._vaddr),
		_packetbuf(std::move(rhs._packetbuf)),
		_stream(std::move(rhs._stream)),
		_sentupstream(std::move(rhs._sentupstream)),
		_sentdownstream(std::move(rhs._sentdownstream)),
		_router(rhs._router)
		{
			this->_stream.rdbuf(&_packetbuf);
		}

		virtual
		~remote_client() {
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
			sys::log_message("nic", "send to _ kernel _ ", this->vaddr(), *kernel);
			try {
				this->_stream << kernel;
			} catch (const Error& err) {
				sys::log_message("nic", "write error _", err);
			} catch (const std::exception& err) {
				sys::log_message("nic", "write error _", err.what());
			} catch (...) {
				sys::log_message("nic", "write error _", "<unknown>");
			}
			/// The kernel is deleted if it goes downstream
			/// and does not carry its parent.
			if (delete_kernel) {
				delete kernel;
			}
		}

		void
		handle(sys::poll_event& event) {
			this->_stream.clear();
			this->_stream.sync();
			kernel_type* kernel = nullptr;
			try {
				while (this->_stream >> kernel) {
					receive_kernel(kernel);
				}
			} catch (const Error& err) {
				sys::log_message("nic", "read error _", err);
			} catch (const std::exception& err) {
				sys::log_message("nic", "read error _", err.what());
			} catch (...) {
				sys::log_message("nic", "read error _", "<unknown>");
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
		operator<<(std::ostream& out, const remote_client& rhs) {
			return out << sys::make_object(
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
				queue_popper(rhs),
				queue_popper(rhs),
				std::bind(&remote_client::recover_kernel, this, _1)
			);
		}

		void
		delete_kernels() {
			do_delete_kernels(_sentupstream);
			do_delete_kernels(_sentdownstream);
		}

		void
		do_delete_kernels(pool_type& rhs) noexcept {
			sys::delete_each(queue_popper(rhs), queue_popper());
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
				instances_guard g(instances);
				auto result = instances.find(k->principal_id());
				if (result == instances.end()) {
					k->result(exit_code::no_principal_found);
					ok = false;
				}
				k->principal(result->second);
			}
			#ifndef NDEBUG
			sys::log_message("nic", "recv _", *k);
			#endif
			if (!ok) {
				return_kernel(k);
			} else {
				_router.send_local(k);
			}
		}

		void return_kernel(kernel_type* k) {
			#ifndef NDEBUG
			sys::log_message("nic", "no principal found for _", *k);
			#endif
			k->principal(k->parent());
			this->send(k);
		}

		void recover_kernel(kernel_type* k) {
			if (k->moves_upstream()) {
				#ifndef NDEBUG
				sys::log_message("nic", "recover _", *k);
				#endif
				_router.send_remote(k);
			} else if (k->moves_somewhere()) {
				#ifndef NDEBUG
				sys::log_message("nic", "destination is unreachable for _", *k);
				#endif
				k->from(k->to());
				k->result(exit_code::endpoint_not_connected);
				k->principal(k->parent());
				_router.send_local(k);
			} else if (k->moves_downstream() and k->carries_parent()) {
				#ifndef NDEBUG
				sys::log_message("nic", "restore parent _", *k);
				#endif
				_router.send_local(k);
			} else {
				sys::log_message("nic", "bad kernel in sent buffer: _", *k);
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
	struct socket_pipeline: public basic_socket_pipeline<T,remote_client<T,Socket,Router>> {

		typedef Socket socket_type;
		typedef Router router_type;

		typedef basic_socket_pipeline<T,remote_client<T,Socket,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;

		using base_pipeline::poller;
		using base_pipeline::send;

		typedef std::map<sys::endpoint,event_handler_ptr> upstream_type;
		typedef typename upstream_type::iterator iterator_type;
		typedef sys::ifaddr<sys::ipv4_addr> network_type;
		typedef network_type::rep_type rep_type;
		typedef mobile_kernel::id_type id_type;

		static_assert(
			std::is_move_constructible<event_handler_type>::value,
			"bad event_handler_type"
		);

		static_assert(
			sizeof(rep_type) <= sizeof(id_type),
			"bad id_type"
		);

		socket_pipeline(socket_pipeline&& rhs) noexcept:
		base_pipeline(std::move(rhs)),
		_network(std::move(rhs._network)),
		_socket(std::move(rhs)),
		_upstream(),
		_iterator(_upstream.end()),
		_counter(rhs._counter),
		_router(rhs._router)
		{}

		socket_pipeline() = default;
		~socket_pipeline() = default;
		socket_pipeline(const socket_pipeline&) = delete;
		socket_pipeline& operator=(const socket_pipeline&) = delete;

		void
		remove_pipeline(event_handler_ptr ptr) override {
			// TODO: occasional ``Bad file descriptor''
			#ifndef NDEBUG
			sys::log_message("nic", "remove _", *ptr);
			#endif
			auto result = _upstream.find(ptr->vaddr());
			if (result != _upstream.end()) {
				remove_valid_pipeline(result);
			}
		}

		void accept_connection(sys::poll_event&) override {
			sys::endpoint addr;
			socket_type sock;
			_socket.accept(sock, addr);
			sys::endpoint vaddr = virtual_addr(addr);
			auto res = _upstream.find(vaddr);
			if (res == _upstream.end()) {
				#ifndef NDEBUG
				event_handler_ptr ptr =
				#endif
				add_connected_pipeline(std::move(sock), vaddr, sys::poll_event::In);
				#ifndef NDEBUG
				sys::log_message("nic", "accept _", *ptr);
				#endif
			} else {
				/*
				event_handler_ptr s = res->second;
				const sys::port_type local_port = s->socket().bind_addr().port();
				if (!(addr.port() < local_port)) {
					#ifndef NDEBUG
					sys::log_message("nic",  "replace _", s);
					#endif
					poller().disable(s->socket().fd());
					s->socket(std::move(sock));
					remove_valid_pipeline(res);
					_upstream.emplace(vaddr, std::move(s));
					poller().emplace(sys::poll_event{s->socket().fd(), sys::poll_event::Inout, sys::poll_event::Inout}, s);
				}
				*/
			}
		}

		void peer(sys::endpoint addr) {
			lock_type lock(this->_mutex);
			this->connect_to_pipeline(addr, sys::poll_event::In);
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
		bind_address() const {
			return this->_socket.bind_addr();
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
			if (not kernel->has_id()) {
				kernel->set_id(generate_id());
			}
		}

		void
		ensure_identity(kernel_type* kernel) {
			ensure_identity_helper(kernel);
			ensure_identity_helper(kernel->parent());
		}

		void
		remove_valid_pipeline(iterator_type result) noexcept {
			if (result == _iterator) {
				advance_upstream_iterator();
			}
			result->second->setstate(pipeline_state::stopped);
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
		emplace_pipeline(const sys::endpoint& vaddr, event_handler_ptr&& s) {
			auto result = _upstream.emplace(vaddr, std::move(s));
			if (_upstream.size() == 1) {
				_iterator = _upstream.begin();
			}
			return result;
		}

		inline sys::endpoint
		virtual_addr(sys::endpoint addr) const {
			return sys::endpoint(addr, bind_address().port());
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
			if (this->bind_address() && k->to() == this->bind_address()) {
				_router.send_local(k);
			}

			if (k->moves_everywhere()) {
				for (auto& pair : _upstream) {
					pair.second->send(k);
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
					if (_iterator->second->is_stopped()) {
						do {
							advance_upstream_iterator();
						} while (_iterator->second->is_stopped() and old_iterator != _iterator);
					}
					// round robin over upstream hosts
					ensure_identity(k);
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
					ensure_identity(k);
				}
				this->find_or_create_peer(k->to(), sys::poll_event::Inout)->send(k);
			}
		}

		event_handler_ptr
		find_or_create_peer(const sys::endpoint& addr, sys::poll_event::legacy_event ev) {
			event_handler_ptr ret;
			auto result = _upstream.find(addr);
			if (result == _upstream.end()) {
				ret = this->connect_to_pipeline(addr, ev);
			} else {
				ret = result->second;
			}
			return ret;
		}

		event_handler_ptr
		connect_to_pipeline(sys::endpoint addr, sys::poll_event::legacy_event events) {
			// bind to server address with ephemeral port
			sys::endpoint srv_addr(this->bind_address(), 0);
			return this->add_connected_pipeline(socket_type(srv_addr, addr), addr, events);
		}

		event_handler_ptr
		add_connected_pipeline(socket_type&& sock, sys::endpoint vaddr,
			sys::poll_event::legacy_event events,
			sys::poll_event::legacy_event revents=0)
		{
			sys::fd_type fd = sock.fd();
			event_handler_ptr s(new event_handler_type(std::move(sock), vaddr, _router));
			// s.setparent(this);
			auto result = emplace_pipeline(vaddr, std::move(s));
			poller().emplace(sys::poll_event{fd, events, revents}, result.first->second);
			#ifndef NDEBUG
			sys::log_message("nic", "add _", result.first->second);
			#endif
			return result.first->second;
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

#endif // vim:filetype=cpp
