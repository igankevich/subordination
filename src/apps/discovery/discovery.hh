#ifndef APPS_DISCOVERY_DISCOVERY_HH
#define APPS_DISCOVERY_DISCOVERY_HH

#include <map>
#include <vector>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <iterator>

#include <stdx/log.hh>

#include <sysx/bits/check.hh>
#include <sysx/endpoint.hh>

namespace discovery {

	template<class Addr>
	struct Network {

		typedef sysx::bits::Const_char<'/'> Slash;
		typedef Addr addr_type;
		typedef typename addr_type::rep_type rep_type;
		typedef typename sysx::ipaddr_traits<addr_type> traits_type;

		constexpr
		Network(const addr_type& addr, const addr_type& netmask) noexcept:
		_address(addr), _netmask(netmask)
		{}

		constexpr Network() noexcept = default;
		constexpr Network(const Network&) noexcept = default;
		constexpr Network(Network&&) noexcept = default;
		Network& operator=(const Network&) noexcept = default;

		constexpr const addr_type&
		address() const noexcept {
			return _address;
		}

		constexpr const addr_type&
		netmask() const noexcept {
			return _netmask;
		}

		constexpr const addr_type&
		gateway() const noexcept {
			return addr_type(start());
		}

		constexpr rep_type
		start() const noexcept {
			return (addr_long() & mask_long()) + 1;
		}

		constexpr rep_type
		end() const noexcept {
			return (addr_long() & mask_long()) + (~mask_long());
		}

		constexpr bool
		is_loopback() const noexcept {
			return _address[0] == traits_type::loopback_first_octet
				and _netmask == traits_type::loopback_mask();
		}

		constexpr bool
		is_widearea() const noexcept {
			return _netmask == traits_type::widearea_mask();
		}

		explicit constexpr
		operator bool() const noexcept {
			return static_cast<bool>(_address) and static_cast<bool>(_netmask);
		}

		constexpr bool
		operator !() const noexcept {
			return !operator bool();
		}

		constexpr bool
		operator==(const Network& rhs) const noexcept {
			return _address == rhs._address && _netmask == rhs._netmask;
		}

		constexpr bool
		operator!=(const Network& rhs) const noexcept {
			return !operator==(rhs);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Network& rhs) {
			return out << rhs._address << Slash() << rhs._netmask;
		}

		friend std::istream&
		operator>>(std::istream& in, Network& rhs) {
			return in >> rhs._address >> Slash() >> rhs._netmask;
		}

	private:

		constexpr const rep_type
		addr_long() const noexcept {
			return sysx::to_host_format(_address.rep());
		}

		constexpr const rep_type
		mask_long() const noexcept {
			return sysx::to_host_format(_netmask.rep());
		}

		addr_type _address;
		addr_type _netmask;
	};

	template<class Addr, class Result>
	void
	enumerate_networks(Result result) {
		sysx::ifaddrs addrs;
		std::for_each(
			addrs.begin(), addrs.end(),
			[&result] (const sysx::ifaddrs::ifaddrs_type& rhs) {
				if (rhs.ifa_addr != 0 and rhs.ifa_addr->sa_family == AF_INET) {
					const Addr addr(*rhs.ifa_addr);
					const Addr netmask(*rhs.ifa_netmask);
					*result = Network<Addr>(addr, netmask);
					++result;
				}
			}
		);
	}

}

#endif // APPS_DISCOVERY_DISCOVERY_HH
