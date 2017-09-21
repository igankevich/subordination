#ifndef BSCHEDULER_PPL_LOCAL_SERVER_HH
#define BSCHEDULER_PPL_LOCAL_SERVER_HH

#include <limits>

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

namespace bsc {

	template <class Addr>
	inline void
	determine_id_range(
		const sys::ifaddr<Addr>& ifaddr,
		typename Addr::rep_type& first,
		typename Addr::rep_type& last
	) noexcept {
		typedef typename Addr::rep_type id_type;
		using std::max;
		using std::min;
		const id_type n = ifaddr.count();
		if (n == 0) {
			first = 0;
			last = 0;
			return;
		}
		const id_type min_id = std::numeric_limits<id_type>::min();
		const id_type max_id = std::numeric_limits<id_type>::max();
		const id_type step = (max_id-min_id) / n;
		const id_type pos = ifaddr.position();
		first = max(id_type(1), min_id + step*(pos-1));
		last = min(max_id, min_id + step*(pos));
	}

	template <class Addr, class Socket>
	class local_server {

	public:
		typedef Addr addr_type;
		typedef Socket socket_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef typename ifaddr_type::rep_type id_type;
		typedef id_type counter_type;

	private:
		ifaddr_type _ifaddr;
		sys::endpoint _endpoint;
		id_type _pos0 = 0;
		id_type _pos1 = 0;
		counter_type _counter{0};
		socket_type _socket;

	public:
		inline explicit
		local_server(const ifaddr_type& ifaddr, sys::port_type port):
		_ifaddr(ifaddr),
		_endpoint(ifaddr.address(), port),
		_socket(sys::endpoint(ifaddr.address(), port))
		{ this->init(); }

		inline
		local_server(local_server&& rhs):
		_ifaddr(rhs._ifaddr),
		_endpoint(rhs._endpoint),
		_pos0(rhs._pos0),
		_pos1(rhs._pos1),
		_counter(rhs._counter),
		_socket(std::move(rhs._socket))
		{}

		local_server() = delete;
		local_server(const local_server&) = delete;
		local_server& operator=(const local_server&) = delete;
		local_server& operator=(local_server&&) = default;

		inline const ifaddr_type
		ifaddr() const noexcept {
			return this->_ifaddr;
		}

		inline const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
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

	private:

		inline void
		init() noexcept {
			determine_id_range(this->_ifaddr, this->_pos0, this->_pos1);
			this->_counter = this->_pos0;
		}

	};

}

#endif // vim:filetype=cpp
