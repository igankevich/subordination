#include "socket_pipeline.hh"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <unistdx/net/socket>

#include <bscheduler/config.hh>
#include <bscheduler/kernel/kernel_instance_registry.hh>
#include <bscheduler/ppl/basic_router.hh>
#include <bscheduler/ppl/kernel_protocol.hh>
#include <bscheduler/ppl/socket_pipeline_event.hh>

namespace {

	template <class kernel, class Server>
	void
	set_kernel_id(kernel* k, Server& srv) {
		if (!k->has_id()) {
			k->set_id(srv.generate_id());
		}
	}

	template <class kernel, class Id>
	void
	set_kernel_id_2(kernel* k, Id& counter) {
		if (!k->has_id()) {
			k->set_id(++counter);
		}
	}

	template <class Router, class ... Args>
	void
	fire_event_kernels(const Args& ... args) {
		using namespace bsc;
		instances_guard g(instances);
		for (const auto& pair : instances) {
			socket_pipeline_kernel* ev = new socket_pipeline_kernel(args ...);
			ev->parent(ev);
			ev->principal(pair.second);
			Router::send_local(ev);
		}
	}

}

namespace bsc {

	template <class K, class S, class R>
	class local_server: public basic_handler {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef K kernel_type;
		typedef S socket_type;
		typedef R router_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef typename ifaddr_type::rep_type id_type;
		typedef id_type counter_type;
		typedef socket_pipeline<K,S,R> this_type;

	private:
		ifaddr_type _ifaddr;
		sys::endpoint _endpoint;
		id_type _pos0 = 0;
		id_type _pos1 = 0;
		counter_type _counter {0};
		socket_type _socket;
		this_type& _ppl;

	public:
		inline explicit
		local_server(
			const ifaddr_type& ifaddr,
			sys::port_type port,
			this_type& ppl
		):
		_ifaddr(ifaddr),
		_endpoint(ifaddr.address(), port),
		_socket(sys::endpoint(ifaddr.address(), port)),
		_ppl(ppl) {
			this->init();
		}

		local_server() = delete;

		local_server(const local_server&) = delete;

		local_server&
		operator=(const local_server&) = delete;

		local_server&
		operator=(local_server&&) = default;

		inline const ifaddr_type
		ifaddr() const noexcept {
			return this->_ifaddr;
		}

		inline const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.get_fd();
		}

		inline const socket_type&
		socket() const noexcept {
			return this->_socket;
		}

		inline socket_type&
		socket() noexcept {
			return this->_socket;
		}

		inline id_type
		generate_id() noexcept {
			id_type id;
			if (this->_counter == this->_pos1) {
				id = this->_pos0;
				this->_counter = id+1;
			} else {
				id = this->_counter++;
			}
			return id;
		}

		void
		handle(const sys::epoll_event& ev) override {
			sys::endpoint addr;
			socket_type sock;
			this->_socket.accept(sock, addr);
			sys::endpoint vaddr = _ppl.virtual_addr(addr);
			auto res = _ppl._clients.find(vaddr);
			if (res == _ppl._clients.end()) {
				#ifndef NDEBUG
				auto ptr =
				#endif
				this->_ppl.do_add_client(std::move(sock), vaddr);
				#ifndef NDEBUG
				this->log("accept _", *ptr);
				#endif
			}
		}

		void
		write(std::ostream& out) const override {
			out << "server " << this->_ifaddr;
		}

		void
		remove(sys::event_poller& poller) override {
			poller.erase(this->fd());
			this->_ppl.remove_server(this->_ifaddr);
		}

	private:

		inline void
		init() noexcept {
			determine_id_range(this->_ifaddr, this->_pos0, this->_pos1);
			this->_counter = this->_pos0;
		}

	};

	template <class K, class S, class R>
	class remote_client: public basic_handler {

	public:
		typedef pipeline_base base_pipeline;
		typedef K kernel_type;
		typedef char Ch;
		typedef sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sys::socket>
		    fildesbuf_type;
		typedef basic_kernelbuf<fildesbuf_type> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<K> stream_type;
		typedef kernel_protocol<K,R,bits::forward_to_child<R>>
		    protocol_type;
		typedef S socket_type;
		typedef R router_type;
		typedef sys::pid_type app_type;
		typedef socket_pipeline<K,S,R> this_type;
		typedef typename this_type::weight_type weight_type;

		static_assert(
			std::is_move_constructible<stream_type>::value,
			"bad stream_type"
		);

	private:
		sys::endpoint _vaddr;
		kernelbuf_ptr _packetbuf;
		stream_type _stream;
		protocol_type _proto;
		/// The number of nodes "behind" this one in the hierarchy.
		weight_type _weight = 1;
		this_type& _ppl;

	public:
		remote_client() = default;

		remote_client(socket_type&& sock, sys::endpoint vaddr, this_type& ppl):
		_vaddr(vaddr),
		_packetbuf(new kernelbuf_type),
		_stream(_packetbuf.get()),
		_proto(),
		_ppl(ppl) {
			this->_proto.setf(kernel_proto_flag::prepend_application);
			this->_proto.set_endpoint(this->_vaddr);
			this->_packetbuf->setfd(std::move(sock));
		}

		remote_client&
		operator=(const remote_client&) = delete;

		remote_client&
		operator=(remote_client&&) = delete;

		remote_client(const remote_client&) = delete;

		remote_client(remote_client&& rhs) = delete;

		virtual
		~remote_client() {
			// Here failed kernels are written to buffer,
			// from which they must be recovered with recover_kernels().
			sys::epoll_event ev {socket().fd(), sys::event::in};
			this->handle(ev);
			// recover kernels from upstream and downstream buffer
			this->_proto.recover_kernels(this->socket().error());
		}

		void
		send(kernel_type* k) {
			this->_proto.send(k, this->_stream);
		}

		void
		forward(kernel_header& hdr, sys::pstream& istr) {
			this->_proto.forward(hdr, istr, this->_stream);
		}

		void
		handle(const sys::epoll_event& event) override {
			if (this->is_starting() && !this->socket().error()) {
				this->setstate(pipeline_state::started);
			}
			if (event.in()) {
				this->_packetbuf->pubfill();
				this->_proto.receive_kernels(this->_stream);
			}
		}

		void
		flush() override {
			if (this->_packetbuf->dirty()) {
				this->_packetbuf->pubflush();
			}
		}

		inline const socket_type&
		socket() const noexcept {
			return this->_packetbuf->fd();
		}

		inline socket_type&
		socket() noexcept {
			return this->_packetbuf->fd();
		}

		void
		socket(sys::socket&& rhs) {
			this->_packetbuf->pubsync();
			this->_packetbuf->setfd(socket_type(std::move(rhs)));
		}

		inline weight_type
		weight() const noexcept {
			return this->_weight;
		}

		inline void
		weight(weight_type rhs) noexcept {
			this->_weight = rhs;
		}

		inline const sys::endpoint&
		vaddr() const noexcept {
			return this->_vaddr;
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->pipeline_base::set_name(rhs);
			this->_proto.set_name(rhs);
			#ifndef NDEBUG
			if (this->_packetbuf) {
				this->_packetbuf->set_name(rhs);
			}
			#endif
		}

		void
		write(std::ostream& out) const override {
			out << "client " << sys::make_object(
				"vaddr",
				this->vaddr(),
				"socket",
				this->socket(),
				"state",
				int(this->state()),
				"weight",
				this->weight(),
				"remaining",
				this->_packetbuf->remaining(),
				"available",
				this->_packetbuf->available()
			    );
		}

		void
		remove(sys::event_poller& poller) override {
			poller.erase(this->_packetbuf->fd().get_fd());
			this->_ppl.remove_client(this->vaddr());
		}

	};

	/*
	template <class K, class S, class R>
	class socket_notify_handler: public basic_handler {

	public:
		typedef socket_pipeline<K,S,R> this_type;

	private:
		this_type& _ppl;

	public:

		explicit
		socket_notify_handler(this_type& ppl):
		_ppl(ppl) {}

		void
		handle(const sys::epoll_event& ev) override {
			this->_ppl.process_kernels();
		}

	};
	*/

}

template <class T, class S, class R>
bsc::socket_pipeline<T,S,R>
::socket_pipeline() {
	using namespace std::chrono;
	this->set_start_timeout(seconds(7));
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::remove_server(const ifaddr_type& ifaddr) {
	lock_type lock(this->_mutex);
	server_iterator result = this->find_server(ifaddr);
	if (result != this->_servers.end()) {
		this->remove_server(result);
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::remove_server(server_iterator result) {
	// copy ifaddr
	ifaddr_type ifaddr = (*result)->ifaddr();
	this->_servers.erase(result);
	fire_event_kernels<router_type>(
		socket_pipeline_event::remove_server,
		ifaddr
	);
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::remove_client(const sys::endpoint& vaddr) {
	client_iterator result = _clients.find(vaddr);
	if (result != this->_clients.end()) {
		this->remove_client(result);
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::remove_client(client_iterator result) {
	// copy endpoint
	sys::endpoint endpoint = result->first;
	#ifndef NDEBUG
	const char* reason =
		result->second->is_starting() ? "timed out" : "connection closed";
	this->log("remove client _ (_)", endpoint, reason);
	#endif
	if (result == this->_iterator) {
		this->advance_client_iterator();
	}
	result->second->setstate(pipeline_state::stopped);
	this->_clients.erase(result);
	fire_event_kernels<router_type>(
		socket_pipeline_event::remove_client,
		endpoint
	);
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::add_server(const sys::endpoint& rhs, addr_type netmask) {
	lock_type lock(this->_mutex);
	ifaddr_type ifaddr(traits_type::address(rhs), netmask);
	if (this->find_server(ifaddr) == this->_servers.end()) {
		auto ptr =
			std::make_shared<server_type>(
				ifaddr,
				traits_type::port(rhs),
				*this
			);
		this->_servers.emplace_back(ptr);
		#ifndef NDEBUG
		this->log("add server _", rhs);
		#endif
		ptr->socket().set_user_timeout(this->_socket_timeout);
		this->emplace_handler(sys::epoll_event(ptr->fd(), sys::event::in), ptr);
		fire_event_kernels<router_type>(
			socket_pipeline_event::add_server,
			ifaddr
		);
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::forward(
	kernel_header& hdr,
	sys::pstream& istr
) {
	// do not lock here as static_lock locks both mutexes
	assert(this->other_mutex());
	assert(hdr.is_foreign());
	if (hdr.to()) {
		event_handler_ptr ptr = this->find_or_create_client(hdr.to());
		this->log("fwd _ to _", hdr, hdr.to());
		ptr->forward(hdr, istr);
		this->_semaphore.notify_one();
	} else {
		if (this->end_reached()) {
			this->find_next_client();
			this->log("fwd _ to _", hdr, "localhost");
			router_type::forward_child(hdr, istr);
		} else {
			this->log("fwd _ to _", hdr, this->current_client().vaddr());
			this->current_client().forward(hdr, istr);
			this->find_next_client();
			this->_semaphore.notify_one();
		}
	}
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::server_iterator
bsc::socket_pipeline<T,S,R>
::find_server(const ifaddr_type& ifaddr) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[&ifaddr] (const value_type& rhs) {
		    return rhs->ifaddr() == ifaddr;
		}
	);
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::server_iterator
bsc::socket_pipeline<T,S,R>
::find_server(sys::fd_type fd) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[fd] (const value_type& rhs) {
		    return rhs->socket().get_fd() == fd;
		}
	);
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::server_iterator
bsc::socket_pipeline<T,S,R>
::find_server(const sys::endpoint& dest) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[&dest] (const value_type& rhs) {
		    return rhs->ifaddr().contains(dest.addr4());
		}
	);
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::ensure_identity(
	kernel_type* k,
	const sys::endpoint& dest
) {
	if (dest.family() == sys::family_type::unix) {
		set_kernel_id_2(k, this->_counter);
		set_kernel_id_2(k->parent(), this->_counter);
	} else {
		if (this->_servers.empty()) {
			k->return_to_parent(
				exit_code::no_upstream_servers_available
			);
		} else {
			server_iterator result = this->find_server(dest);
			if (result == this->_servers.end()) {
				result = this->_servers.begin();
			}
			set_kernel_id(k, **result);
			set_kernel_id(k->parent(), **result);
		}
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::find_next_client() {
	if (this->_clients.empty()) {
		return;
	}
	client_iterator old_iterator = this->_iterator;
	do {
		// wrap iterator
		if (this->end_reached()) {
			this->_iterator = this->_clients.begin();
		}
		// increment
		if (!this->end_reached()) {
			if (this->_weightcnt < this->current_client().weight()) {
				++this->_weightcnt;
			} else {
				this->advance_client_iterator();
			}
		}
		// include localhost into round-robin
		if (this->_uselocalhost && this->end_reached()) {
			break;
		}
		// skip stopped hosts
		if (!this->end_reached() && this->current_client().has_started()) {
			break;
		}
	} while (old_iterator != this->_iterator);
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::emplace_client(const sys::endpoint& vaddr, const event_handler_ptr& s) {
	const bool save = !this->end_reached();
	sys::endpoint e;
	if (save) {
		e = this->_iterator->first;
	}
	this->_clients.emplace(vaddr, s);
	if (save) {
		this->_iterator = this->_clients.find(e);
	} else {
		this->reset_iterator();
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::process_kernels() {
//	lock_type lock(this->_mutex);
	std::for_each(
		sys::queue_popper(this->_kernels),
		sys::queue_popper_end(this->_kernels),
		[this] (kernel_type* k) {
		    try {
		        this->process_kernel(k);
			} catch (const std::exception& err) {
		        this->log_error(err);
		        k->from(k->to());
		        k->return_to_parent(exit_code::no_upstream_servers_available);
		        router_type::send_local(k);
			}
		}
	);
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::process_kernel(kernel_type* k) {
	// short circuit local server
	/*
	   if (k->to()) {
	    server_iterator result = this->find_server(k->to());
	    if (result != this->_servers.end()) {
	        k->from(k->to());
	        k->return_to_parent(exit_code::no_upstream_servers_available);
	        router_type::send_local(k);
	        return;
	    }
	   }
	 */
	if (k->moves_everywhere()) {
		for (auto& pair : _clients) {
			pair.second->send(k);
		}
		// delete broadcast kernel
		delete k;
	} else if (k->moves_upstream() && k->to() == sys::endpoint()) {
		bool success = false;
		if (this->_uselocalhost) {
			if (this->end_reached()) {
				// include localhost in round-robin
				// (short-circuit kernels when no upstream servers
				// are available)
				router_type::send_local(k);
			} else {
				success = true;
			}
		} else {
			if (this->_clients.empty()) {
				k->return_to_parent(exit_code::no_upstream_servers_available);
				router_type::send_local(k);
			} else {
				success = true;
			}
		}
		if (success) {
			ensure_identity(k, this->_iterator->second->vaddr());
			this->_iterator->second->send(k);
		}
		this->find_next_client();
	} else if (k->moves_downstream() and not k->from()) {
		// kernel @k was sent to local node
		// because no upstream servers had
		// been available
		router_type::send_local(k);
	} else {
		// create endpoint if necessary, and send kernel
		if (not k->to()) {
			k->to(k->from());
		}
		if (k->moves_somewhere()) {
			ensure_identity(k, k->to());
		}
		this->find_or_create_client(k->to())->send(k);
	}
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::event_handler_ptr
bsc::socket_pipeline<T,S,R>
::find_or_create_client(const sys::endpoint& addr) {
	event_handler_ptr ret;
	auto result = _clients.find(addr);
	if (result == _clients.end()) {
		ret = this->do_add_client(addr);
	} else {
		ret = result->second;
	}
	return ret;
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::event_handler_ptr
bsc::socket_pipeline<T,S,R>
::do_add_client(const sys::endpoint& addr) {
	if (addr.family() == sys::family_type::unix) {
		socket_type s(sys::family_type::unix);
		s.setopt(socket_type::pass_credentials);
		s.connect(addr);
		return this->do_add_client(std::move(s), addr);
	} else {
		server_iterator result = this->find_server(addr);
		if (result == this->_servers.end()) {
			throw std::invalid_argument("no matching server found");
		}
		// bind to server address with ephemeral port
		sys::endpoint srv_addr((*result)->endpoint(), 0);
		return this->do_add_client(socket_type(srv_addr, addr), addr);
	}
}

template <class T, class S, class R>
typename bsc::socket_pipeline<T,S,R>::event_handler_ptr
bsc::socket_pipeline<T,S,R>
::do_add_client(socket_type&& sock, sys::endpoint vaddr) {
	sys::fd_type fd = sock.fd();
	if (vaddr.family() != sys::family_type::unix) {
		sock.set_user_timeout(this->_socket_timeout);
	}
	event_handler_ptr s =
		std::make_shared<event_handler_type>(
			std::move(sock),
			vaddr,
			*this
		);
	s->setstate(pipeline_state::starting);
	s->set_name(this->_name);
	this->emplace_client(vaddr, s);
	this->emplace_handler(sys::epoll_event(fd, sys::event::inout), s);
	fire_event_kernels<router_type>(
		socket_pipeline_event::add_client,
		vaddr
	);
	return s;
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::stop_client(const sys::endpoint& addr) {
	lock_type lock(this->_mutex);
	client_iterator result = this->_clients.find(addr);
	if (result != this->_clients.end()) {
		result->second->setstate(pipeline_state::stopped);
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::set_client_weight(
	const sys::endpoint& addr,
	weight_type new_weight
) {
	lock_type lock(this->_mutex);
	client_iterator result = this->_clients.find(addr);
	if (result != this->_clients.end()) {
		result->second->weight(new_weight);
	}
}

template <class T, class S, class R>
void
bsc::socket_pipeline<T,S,R>
::print_state(std::ostream& out) {
	typedef typename server_container_type::value_type server_type;
	typedef typename client_container_type::value_type client_pair;
	lock_type lock(this->_mutex);
	for (const server_type& val : this->_servers) {
		this->log("server _", val);
	}
	for (const client_pair& val : this->_clients) {
		this->log("client _, handler _", val.first, *val.second);
	}
}

template class bsc::socket_pipeline<
		BSCHEDULER_KERNEL_TYPE,
		sys::socket,
		bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
