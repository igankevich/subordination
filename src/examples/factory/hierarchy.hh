#ifndef APPS_FACTORY_HIERARCHY_HH
#define APPS_FACTORY_HIERARCHY_HH

#include <iosfwd>
#include <unordered_set>

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

namespace factory {

	template<class Addr>
	class hierarchy {

	public:
		typedef Addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef std::unordered_set<sys::endpoint> container_type;
		typedef typename container_type::const_iterator const_iterator;
		typedef typename container_type::size_type size_type;

	protected:
		ifaddr_type _ifaddr;
		sys::endpoint _endpoint;
		sys::endpoint _principal;
		container_type _subordinates;

	public:

		hierarchy(const ifaddr_type& ifaddr, const sys::port_type port):
		_ifaddr(ifaddr),
		_endpoint(ifaddr.address(), port),
		_principal(),
		_subordinates()
		{}

		hierarchy(const hierarchy&) = default;
		hierarchy(hierarchy&&) = default;

		const ifaddr_type&
		ifaddr() const noexcept {
			return this->_ifaddr;
		}

		const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

		const sys::endpoint&
		principal() const noexcept {
			return this->_principal;
		}

		void
		unset_principal() noexcept {
			this->_principal.reset();
		}

		void
		set_principal(const sys::endpoint& new_princ) {
			this->_principal = new_princ;
			this->_subordinates.erase(new_princ);
		}

		void
		add_subordinate(const sys::endpoint& addr) {
			this->_subordinates.insert(addr);
		}

		bool
		remove_subordinate(const sys::endpoint& addr) {
			return this->_subordinates.erase(addr) > 0;
		}

		size_type
		num_subordinates() const noexcept {
			return this->_subordinates.size();
		}

		bool
		has_principal() const noexcept {
			return static_cast<bool>(this->_principal);
		}

		size_type
		num_neighbours() const noexcept {
			return this->num_subordinates() + (this->has_principal() ? 1 : 0);
		}

		sys::port_type
		port() const noexcept {
			return sys::ipaddr_traits<addr_type>::port(this->_endpoint);
		}

		const_iterator
		begin() const noexcept {
			return this->_subordinates.begin();
		}

		const_iterator
		end() const noexcept {
			return this->_subordinates.end();
		}

		template <class X>
		friend std::ostream&
		operator<<(std::ostream& out, const hierarchy<X>& rhs);

	};

	template <class X>
	std::ostream&
	operator<<(std::ostream& out, const hierarchy<X>& rhs);

}

#endif // vim:filetype=cpp
