#include "socket_pipeline.hh"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <factory/config.hh>
#include <factory/ppl/basic_router.hh>

namespace {

	template <class Kernel, class Server>
	void
	set_kernel_id(Kernel* kernel, Server& srv) {
		if (!kernel->has_id()) {
			kernel->set_id(srv.generate_id());
		}
	}

	template <class Kernel, class Id>
	void
	set_kernel_id_2(Kernel* kernel, Id& counter) {
		if (!kernel->has_id()) {
			kernel->set_id(++counter);
		}
	}

}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::remove_server(sys::fd_type fd) {
	server_iterator result = this->find_server(fd);
	if (result != this->_servers.end()) {
		this->remove_server(result);
	}
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::remove_server(const ifaddr_type& ifaddr) {
	lock_type lock(this->_mutex);
	server_iterator result = this->find_server(ifaddr);
	if (result != this->_servers.end()) {
		this->remove_server(result);
	}
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::remove_server(server_iterator result) {
	#ifndef NDEBUG
	sys::log_message(this->_name, "remove server _", result->ifaddr());
	#endif
	this->_servers.erase(result);
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::remove_client(event_handler_ptr ptr) {
	client_iterator result = _clients.find(ptr->vaddr());
	if (result != this->_clients.end()) {
		this->remove_client(result);
	}
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::remove_client(client_iterator result) {
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
		++this->_iterator;
	}
	result->second->setstate(pipeline_state::stopped);
	this->_clients.erase(result);
}


template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::accept_connection(sys::poll_event& ev) {
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

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::add_server(
	const sys::endpoint& rhs,
	addr_type netmask
) {
	lock_type lock(this->_mutex);
	ifaddr_type ifaddr(traits_type::address(rhs), netmask);
	if (this->find_server(ifaddr) == this->_servers.end()) {
		this->_servers.emplace_back(ifaddr, traits_type::port(rhs));
		server_type& srv = this->_servers.back();
		#ifndef NDEBUG
		this->log("add server _", rhs);
		#endif
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

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::forward(
	const kernel_header& hdr,
	sys::pstream& istr
) {
	lock_type lock(this->_mutex);
	this->log("fwd _", hdr);
	assert(hdr.is_foreign());
	if (hdr.to()) {
		event_handler_ptr ptr =
			this->find_or_create_peer(hdr.to(), sys::poll_event::Inout);
		ptr->forward(hdr, istr);
	} else {
		this->find_next_client();
		if (this->end_reached()) {
			this->reset_iterator();
			this->_mutex.unlock();
			router_type::forward_child(hdr, istr);
		} else {
			this->_iterator->second->forward(hdr, istr);
			++this->_iterator;
		}
	}
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::server_iterator
factory::socket_pipeline<T,S,R>::find_server(const ifaddr_type& ifaddr) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[&ifaddr] (const value_type& rhs) {
			return rhs.ifaddr() == ifaddr;
		}
	);
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::server_iterator
factory::socket_pipeline<T,S,R>::find_server(sys::fd_type fd) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[fd] (const value_type& rhs) {
			return rhs.socket().get_fd() == fd;
		}
	);
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::server_iterator
factory::socket_pipeline<T,S,R>::find_server(const sys::endpoint& dest) {
	typedef typename server_container_type::value_type value_type;
	return std::find_if(
		this->_servers.begin(),
		this->_servers.end(),
		[&dest] (const value_type& rhs) {
			return rhs.ifaddr().contains(dest.addr4());
		}
	);
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::ensure_identity(
	kernel_type* kernel,
	const sys::endpoint& dest
) {
	if (dest.family() == sys::family_type::unix) {
		set_kernel_id_2(kernel, this->_counter);
		set_kernel_id_2(kernel->parent(), this->_counter);
	} else {
		if (this->_servers.empty()) {
			kernel->return_to_parent(
				exit_code::no_upstream_servers_available
			);
		} else {
			server_iterator result = this->find_server(dest);
			if (result == this->_servers.end()) {
				result = this->_servers.begin();
			}
			set_kernel_id(kernel, *result);
			set_kernel_id(kernel->parent(), *result);
		}
	}
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::find_next_client() {
	if (this->_clients.empty()) {
		return;
	}
	// skip stopped hosts
	client_iterator old_iterator = this->_iterator;
	do {
		if (this->_iterator == this->_clients.end()) {
			this->_iterator = this->_clients.begin();
		} else {
			++this->_iterator;
		}
		if (this->_iterator != this->_clients.end() &&
			this->_iterator->second->is_running()) {
			break;
		}
	} while (old_iterator != this->_iterator);
}

template <class T, class S, class R>
std::pair<typename factory::socket_pipeline<T,S,R>::client_iterator,bool>
factory::socket_pipeline<T,S,R>::emplace_pipeline(
	const sys::endpoint& vaddr,
	event_handler_ptr&& s
) {
	const bool save = !this->end_reached();
	sys::endpoint e;
	if (save) {
		e = this->_iterator->first;
	}
	auto result = this->_clients.emplace(vaddr, std::move(s));
	if (save) {
		this->_iterator = this->_clients.find(e);
	} else {
		this->_iterator = result.first;
	}
	return result;
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::process_kernels() {
	lock_type lock(this->_mutex);
	std::for_each(
		sys::queue_popper(this->_kernels),
		sys::queue_popper_end(this->_kernels),
		[this] (kernel_type* rhs) { process_kernel(rhs); }
	);
}

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::process_kernel(kernel_type* k) {
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
		this->find_next_client();
		if (this->_uselocalhost) {
			if (this->end_reached()) {
				this->reset_iterator();
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
			} else if (this->end_reached()) {
				this->reset_iterator();
				success = true;
			} else {
				success = true;
			}
		}
		if (success) {
			ensure_identity(k, this->_iterator->second->vaddr());
			this->_iterator->second->send(k);
			++this->_iterator;
		}
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
		this->find_or_create_peer(k->to(), sys::poll_event::Inout)->send(k);
	}
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::event_handler_ptr
factory::socket_pipeline<T,S,R>::find_or_create_peer(
	const sys::endpoint& addr,
	sys::poll_event::legacy_event ev
) {
	event_handler_ptr ret;
	auto result = _clients.find(addr);
	if (result == _clients.end()) {
		ret = this->add_client(addr, ev);
	} else {
		ret = result->second;
	}
	return ret;
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::event_handler_ptr
factory::socket_pipeline<T,S,R>::add_client(
	const sys::endpoint& addr,
	sys::poll_event::legacy_event events
) {
	if (addr.family() == sys::family_type::unix) {
		socket_type s(sys::family_type::unix);
		s.setopt(socket_type::pass_credentials);
		s.connect(addr);
		return this->add_connected_pipeline(std::move(s), addr, events);
	} else {
		server_iterator result = this->find_server(addr);
		if (result == this->_servers.end()) {
			throw std::invalid_argument("no matching server found");
		}
		// bind to server address with ephemeral port
		sys::endpoint srv_addr(result->endpoint(), 0);
		return this->add_connected_pipeline(
			socket_type(srv_addr, addr),
			addr,
			events
		);
	}
}

template <class T, class S, class R>
typename factory::socket_pipeline<T,S,R>::event_handler_ptr
factory::socket_pipeline<T,S,R>::add_connected_pipeline(
	socket_type&& sock,
	sys::endpoint vaddr,
	sys::poll_event::legacy_event events,
	sys::poll_event::legacy_event revents
) {
	sys::fd_type fd = sock.fd();
	if (vaddr.family() != sys::family_type::unix) {
		sock.set_user_timeout(this->_socket_timeout);
	}
	event_handler_ptr s =
		std::make_shared<event_handler_type>(
			std::move(sock),
			vaddr
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

template <class T, class S, class R>
void
factory::socket_pipeline<T,S,R>::stop_client(const sys::endpoint& addr) {
	lock_type lock(this->_mutex);
	client_iterator result = this->_clients.find(addr);
	if (result != this->_clients.end()) {
		result->second->setstate(pipeline_state::stopped);
	}
}

template class factory::socket_pipeline<
	FACTORY_KERNEL_TYPE,
	sys::socket,
	factory::basic_router<FACTORY_KERNEL_TYPE>>;
