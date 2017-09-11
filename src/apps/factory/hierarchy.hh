#ifndef APPS_FACTORY_HIERARCHY_HH
#define APPS_FACTORY_HIERARCHY_HH

#include <iosfwd>
#include <unordered_set>

#ifndef NDEBUG
#include <unistdx/base/log_message>
#endif
#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

namespace factory {

	template<class Addr>
	class hierarchy {

	public:
		typedef Addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef std::unordered_set<sys::endpoint> container_type;
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
			#ifndef NDEBUG
			sys::log_message("dscvr", "set principal to _", new_princ);
			#endif
			this->_principal = new_princ;
			this->_subordinates.erase(new_princ);
		}

		void
		add_subordinate(const sys::endpoint& addr) {
			#ifndef NDEBUG
			sys::log_message("dscvr", "add subordinate _", addr);
			#endif
			this->_subordinates.insert(addr);
		}

		void
		remove_subordinate(const sys::endpoint& addr) {
			#ifndef NDEBUG
			sys::log_message("dscvr", "remove subordinate _", addr);
			#endif
			this->_subordinates.erase(addr);
		}

		size_type
		num_subordinates() const noexcept {
			return this->_subordinates.size();
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
