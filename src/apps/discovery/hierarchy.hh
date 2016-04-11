#ifndef APPS_DISCOVERY_HIERARCHY_HH
#define APPS_DISCOVERY_HIERARCHY_HH

#include <map>
#include <set>

#include <stdx/log.hh>

#include <sys/net/endpoint.hh>
#include <sys/net/ifaddr.hh>

namespace discovery {

	template<class Addr>
	struct Hierarchy {

		typedef Addr addr_type;
		typedef sys::ifaddr<addr_type> network_type;
		typedef std::set<sys::endpoint>::size_type size_type;
		typedef stdx::log<Hierarchy> this_log;

		explicit
		Hierarchy(const network_type& net, const sys::port_type port):
		_network(net),
		_bindaddr(net.address(), port),
		_principal(),
		_subordinates()
		{}

		Hierarchy(const Hierarchy&) = default;
		Hierarchy(Hierarchy&&) = default;

		const network_type&
		network() const noexcept {
			return _network;
		}

		const sys::endpoint&
		bindaddr() const noexcept {
			return _bindaddr;
		}

		const sys::endpoint&
		principal() const noexcept {
			return _principal;
		}

		void
		unset_principal() noexcept {
			_principal.reset();
		}

		void
		set_principal(const sys::endpoint& new_princ) {
			this_log() << "Set principal to " << new_princ << std::endl;
			_principal = new_princ;
			_subordinates.erase(new_princ);
		}

		void
		add_subordinate(const sys::endpoint& addr) {
			this_log() << "Adding subordinate = " << addr << std::endl;
			_subordinates.insert(addr);
		}

		void
		remove_subordinate(const sys::endpoint& addr) {
			this_log() << "Removing subordinate = " << addr << std::endl;
			_subordinates.erase(addr);
		}

		size_type
		num_subordinates() const noexcept {
			return _subordinates.size();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Hierarchy& rhs) {
			out << "network=" << rhs._network << ',';
			out << "principal=" << rhs._principal << ',';
			out << "subordinates=";
			std::copy(
				rhs._subordinates.begin(),
				rhs._subordinates.end(),
				std::ostream_iterator<sys::endpoint>(out, ",")
			);
			return out;
		}

	protected:

		network_type _network;
		sys::endpoint _bindaddr;
		sys::endpoint _principal;
		std::set<sys::endpoint> _subordinates;

	};

}

#endif // APPS_DISCOVERY_HIERARCHY_HH
