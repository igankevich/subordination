#ifndef APPS_FACTORY_PROBE_HH
#define APPS_FACTORY_PROBE_HH

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <factory/api.hh>

namespace factory {

	class probe: public factory::api::Kernel {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;

	private:
		ifaddr_type _ifaddr;
		sys::endpoint _oldprinc;
		sys::endpoint _newprinc;

	public:

		probe() = default;

		inline
		probe(
			const ifaddr_type& ifaddr,
			const sys::endpoint& oldprinc,
			const sys::endpoint& newprinc
		):
		_ifaddr(ifaddr),
		_oldprinc(oldprinc),
		_newprinc(newprinc)
		{}

		void
		write(sys::pstream& out) override;

		void
		read(sys::pstream& in) override;

		inline const sys::endpoint&
		new_principal() const noexcept {
			return this->_newprinc;
		}

		inline const sys::endpoint&
		old_principal() const noexcept {
			return this->_oldprinc;
		}

		inline const ifaddr_type&
		ifaddr() const noexcept {
			return this->_ifaddr;
		}

	};

}

#endif // vim:filetype=cpp
