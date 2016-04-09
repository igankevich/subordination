#ifndef SYSX_NETWORK_HH
#define SYSX_NETWORK_HH

#include <map>
#include <vector>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <iterator>

#include <stdx/log.hh>

#include <sys/bits/check.hh>
#include <sys/endpoint.hh>
#include <sys/ifaddrs.hh>

#include <sys/network_iterator.hh>

namespace sys {

	template<class Addr>
	struct network {

		typedef sys::bits::Const_char<'/'> Slash;
		typedef Addr addr_type;
		typedef typename addr_type::rep_type rep_type;
		typedef typename sys::ipaddr_traits<addr_type> traits_type;
		typedef network_iterator<addr_type> iterator;

		constexpr
		network(const addr_type& addr, const addr_type& netmask) noexcept:
		_address(addr), _netmask(netmask)
		{}

		constexpr
		network(const addr_type& addr, const sys::prefix_type prefix) noexcept:
		_address(addr), _netmask(addr_type::from_prefix(prefix))
		{}

		constexpr network() noexcept = default;
		constexpr network(const network&) noexcept = default;
		constexpr network(network&&) noexcept = default;
		network& operator=(const network&) noexcept = default;

		constexpr const addr_type&
		address() const noexcept {
			return _address;
		}

		constexpr const addr_type&
		netmask() const noexcept {
			return _netmask;
		}

		sys::prefix_type
		prefix() const noexcept {
			return _netmask.to_prefix();
		}

		constexpr const addr_type&
		gateway() const noexcept {
			return addr_type(first());
		}

		constexpr rep_type
		position() const noexcept {
			return _address.position(_netmask);
		}

		constexpr iterator
		begin() const noexcept {
			return iterator(first());
		}

		constexpr iterator
		middle() const noexcept {
			return iterator(_address);
		}

		constexpr iterator
		end() const noexcept {
			return iterator(last());
		}

		constexpr rep_type
		count() const noexcept {
			return last() - first();
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
		operator==(const network& rhs) const noexcept {
			return _address == rhs._address && _netmask == rhs._netmask;
		}

		constexpr bool
		operator!=(const network& rhs) const noexcept {
			return !operator==(rhs);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const network& rhs) {
			return out << rhs._address << Slash() << rhs._netmask.to_prefix();
		}

		friend std::istream&
		operator>>(std::istream& in, network& rhs) {
			sys::prefix_type prefix = 0;
			in >> rhs._address >> Slash() >> prefix;
			rhs._netmask = addr_type::from_prefix(prefix);
			return in;
		}

	private:

		constexpr rep_type
		first() const noexcept {
			return (addr_long() & mask_long()) + 1;
		}

		constexpr rep_type
		last() const noexcept {
			return (addr_long() & mask_long()) + (~mask_long());
		}

		constexpr const rep_type
		addr_long() const noexcept {
			return sys::to_host_format(_address.rep());
		}

		constexpr const rep_type
		mask_long() const noexcept {
			return sys::to_host_format(_netmask.rep());
		}

		addr_type _address;
		addr_type _netmask;
	};

	template<class Addr, class Result>
	void
	enumerate_networks(Result result) {
		sys::ifaddrs addrs;
		std::for_each(
			addrs.begin(), addrs.end(),
			[&result] (const sys::ifaddrs_type& rhs) {
				if (rhs.ifa_addr != 0 and rhs.ifa_addr->sa_family == AF_INET) {
					const Addr addr(*rhs.ifa_addr);
					const Addr netmask(*rhs.ifa_netmask);
					*result = network<Addr>(addr, netmask);
					++result;
				}
			}
		);
	}

}

#endif // SYSX_NETWORK_HH
