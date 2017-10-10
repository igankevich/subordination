#include "unix_domain_socket_pipeline.hh"

#include <bscheduler/config.hh>
#include <bscheduler/ppl/basic_router.hh>

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::remove_server(sys::fd_type fd) {
	server_iterator result = this->find_server(fd);
	if (result != this->_servers.end()) {
		#ifndef NDEBUG
		this->log("remove server _", result->first);
		#endif
		this->_servers.erase(result);
	}
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::remove_client(event_handler_ptr ptr) {
	#ifndef NDEBUG
	this->log("remove _ _", ptr->endpoint(), ptr->socket());
	#endif
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::accept_connection(sys::poll_event& ev) {
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

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::process_kernels() {
//	lock_type lock(this->_mutex);
	std::for_each(
		queue_popper(this->_kernels),
		queue_popper(),
		[this] (kernel_type* rhs) { this->process_kernel(rhs); }
	);
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::process_kernel(kernel_type* k) {
	#ifndef NDEBUG
	this->log("send _", *k);
	#endif
}

template <class K, class R>
typename bsc::unix_domain_socket_pipeline<K,R>::server_iterator
bsc::unix_domain_socket_pipeline<K,R>
::find_server(const sys::endpoint& e) {
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[&e] (const server_type& rhs) {
			return rhs.first == e;
		}
	);
}

template <class K, class R>
typename bsc::unix_domain_socket_pipeline<K,R>::server_iterator
bsc::unix_domain_socket_pipeline<K,R>
::find_server(sys::fd_type fd) {
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[fd] (const server_type& rhs) {
			return rhs.second.get_fd() == fd;
		}
	);
}

template <class K, class R>
typename bsc::unix_domain_socket_pipeline<K,R>::client_iterator
bsc::unix_domain_socket_pipeline<K,R>
::find_client(const sys::endpoint& e) {
	return std::find_if(
		this->poller().begin(),
		this->poller().end(),
		[&e] (const client_type& rhs) {
			return rhs->endpoint() == e;
		}
	);
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_client(const sys::endpoint& addr, sys::socket&& sock) {
	sys::fd_type fd = sock.fd();
	event_handler_ptr s =
		std::make_shared<event_handler_type>(
			addr,
			std::move(sock)
		);
	s->setstate(pipeline_state::starting);
	this->poller().emplace(
		sys::poll_event {fd, sys::poll_event::In, sys::poll_event::In},
		s
	);
	#ifndef NDEBUG
	this->log("add _", addr);
	#endif
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_server(const sys::endpoint& rhs) {
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
			sys::poll_event {fd, sys::poll_event::In}
		);
		if (!this->has_stopped()) {
			this->_semaphore.notify_one();
		}
	}
}

template <class K, class R>
void
bsc::unix_domain_socket_pipeline<K,R>
::add_client(const sys::endpoint& addr) {
	lock_type lock(this->_mutex);
	sys::socket s(sys::family_type::unix);
	s.setopt(sys::socket::pass_credentials);
	s.connect(addr);
	this->add_client(addr, std::move(s));
	this->poller().notify_one();
}

template class bsc::unix_domain_socket_pipeline<
		BSCHEDULER_KERNEL_TYPE, bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
