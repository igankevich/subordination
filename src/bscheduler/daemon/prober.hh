#ifndef BSCHEDULER_DAEMON_PROBER_HH
#define BSCHEDULER_DAEMON_PROBER_HH

#include <unistdx/net/socket_address>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <bscheduler/api.hh>

#include "probe.hh"

namespace bsc {

	class prober: public bsc::kernel {

	public:
		typedef sys::ipv4_address addr_type;
		typedef sys::interface_address<addr_type> ifaddr_type;

	private:
		ifaddr_type _ifaddr;
		sys::socket_address _oldprinc;
		sys::socket_address _newprinc;
		int _nprobes = 0;

	public:

		inline
		prober(
			const ifaddr_type& interface_address,
			const sys::socket_address& oldprinc,
			const sys::socket_address& newprinc
		):
		_ifaddr(interface_address),
		_oldprinc(oldprinc),
		_newprinc(newprinc)
		{}

		void
		act() override;

		void
		react(bsc::kernel* k) override;

		inline const sys::socket_address&
		new_principal() const noexcept {
			return this->_newprinc;
		}

		inline const sys::socket_address&
		old_principal() const noexcept {
			return this->_oldprinc;
		}

	private:

		void
		send_probe(const sys::socket_address& dest);

	};

}

#endif // vim:filetype=cpp
