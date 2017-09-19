#ifndef APPS_FACTORY_HIERARCHY_HH
#define APPS_FACTORY_HIERARCHY_HH

#include <iosfwd>
#include <unordered_set>

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

#include "hierarchy_node.hh"

namespace factory {

	template<class Addr>
	class hierarchy {

	public:
		typedef Addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef std::unordered_set<hierarchy_node> container_type;
		typedef typename container_type::const_iterator const_iterator;
		typedef typename container_type::size_type size_type;
		typedef hierarchy_node node_type;
		typedef node_type::weight_type weight_type;

	protected:
		ifaddr_type _ifaddr;
		sys::endpoint _endpoint;
		hierarchy_node _principal;
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

		const node_type&
		principal() const noexcept {
			return this->_principal;
		}

		void
		unset_principal() noexcept {
			this->_principal.reset();
		}

		void
		set_principal(const sys::endpoint& new_princ) {
			this->_principal.endpoint(new_princ);
			this->_subordinates.erase(this->_principal);
		}

		void
		add_subordinate(const sys::endpoint& addr) {
			this->_subordinates.emplace(addr);
		}

		bool
		remove_subordinate(const sys::endpoint& addr) {
			return this->_subordinates.erase(node_type(addr)) > 0;
		}

		size_type
		num_subordinates() const noexcept {
			return this->_subordinates.size();
		}

		bool
		has_principal() const noexcept {
			return static_cast<bool>(this->_principal.endpoint());
		}

		bool
		has_principal(const sys::endpoint& rhs) const noexcept {
			return this->_principal.endpoint() == rhs;
		}

		bool
		has_subordinate(const sys::endpoint& rhs) const noexcept {
			return this->_subordinates.find(node_type(rhs)) !=
			       this->_subordinates.end();
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

		inline bool
		set_principal_weight(weight_type w) noexcept {
			weight_type old = this->_principal.weight();
			bool changed = old != w;
			if (changed) {
				this->_principal.weight(w);
			}
			return changed;
		}

		/// @return total weight of all subordinate and principal nodes
		weight_type
		total_weight() const noexcept;

		/// @return total weight of all subordinate nodes
		weight_type
		total_subordinate_weight() const noexcept;

		inline weight_type
		principal_weight() const noexcept {
			return this->has_principal() ? this->_principal.weight() : 0;
		}

		bool
		set_subordinate_weight(const sys::endpoint& endp, weight_type w);

		template <class X>
		friend std::ostream&
		operator<<(std::ostream& out, const hierarchy<X>& rhs);

	};

	template <class X>
	std::ostream&
	operator<<(std::ostream& out, const hierarchy<X>& rhs);

}


#endif // vim:filetype=cpp
