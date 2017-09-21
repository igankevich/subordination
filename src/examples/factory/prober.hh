#ifndef APPS_FACTORY_PROBER_HH
#define APPS_FACTORY_PROBER_HH

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <factory/api.hh>

#include "probe.hh"

namespace asc {

	class prober: public asc::kernel {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;

	private:
		ifaddr_type _ifaddr;
		sys::endpoint _oldprinc;
		sys::endpoint _newprinc;
		int _nprobes = 0;

	public:

		inline
		prober(
			const ifaddr_type& ifaddr,
			const sys::endpoint& oldprinc,
			const sys::endpoint& newprinc
		):
		_ifaddr(ifaddr),
		_oldprinc(oldprinc),
		_newprinc(newprinc)
		{}

		void
		act() override;

		void
		react(asc::kernel* k) override;

		inline const sys::endpoint&
		new_principal() const noexcept {
			return this->_newprinc;
		}

		inline const sys::endpoint&
		old_principal() const noexcept {
			return this->_oldprinc;
		}

	private:

		void
		send_probe(const sys::endpoint& dest);

	};

}

#endif // vim:filetype=cpp
