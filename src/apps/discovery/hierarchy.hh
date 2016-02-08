#ifndef APPS_DISCOVERY_PEERS_HH
#define APPS_DISCOVERY_PEERS_HH

#include <map>
#include <set>

#include <stdx/log.hh>

#include <sysx/endpoint.hh>

#include "discovery.hh"

namespace discovery {

	template<class Addr>
	struct Hierarchy {

		typedef Addr addr_type;
		typedef Network<addr_type> network_type;
		typedef std::set<sysx::endpoint>::size_type size_type;
		typedef stdx::log<Hierarchy> this_log;

		explicit
		Hierarchy(const network_type& net):
		_network(net),
		_principal(),
		_subordinates()
		{}

		Hierarchy(const Hierarchy&) = default;
		Hierarchy(Hierarchy&&) = default;

		const network_type&
		network() const noexcept {
			return _network;
		}

		const sysx::endpoint&
		principal() const noexcept {
			return _principal;
		}

		void
		unset_principal() noexcept {
			_principal.reset();
		}

		void
		set_principal(const sysx::endpoint& new_princ) {
			this_log() << "Set principal to " << new_princ << std::endl;
			_principal = new_princ;
			_subordinates.erase(new_princ);
		}

		void
		add_subordinate(const sysx::endpoint& addr) {
			this_log() << "Adding subordinate = " << addr << std::endl;
			_subordinates.insert(addr);
		}

		void
		remove_subordinate(const sysx::endpoint& addr) {
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
				std::ostream_iterator<sysx::endpoint>(out, ",")
			);
			return out;
		}

	protected:

		network_type _network;
		sysx::endpoint _principal;
		std::set<sysx::endpoint> _subordinates;

	};

}

#endif // APPS_DISCOVERY_PEERS_HH
