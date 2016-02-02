#ifndef APPS_DISCOVERY_DISCOVERY_HH
#define APPS_DISCOVERY_DISCOVERY_HH

#include <map>
#include <vector>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <bitset>

#include <stdx/log.hh>

#include <sysx/bits/check.hh>
#include <sysx/endpoint.hh>

#include "interval_set.hh"

namespace discovery {

	enum Network_flags_t {
		include_loopback = 0,
		include_local    = 1,
		include_global   = 2,
		exclude_right    = 3,
		exclude_left     = 4
	};

	typedef std::bitset<5> Network_flags;

	template<class Function>
	void
	for_each_ifaddr(Function func, Network_flags flags) {
		sysx::ifaddrs addrs;
		std::for_each(
			addrs.begin(), addrs.end(),
			[flags,&func] (const sysx::ifaddrs::ifaddrs_type& rhs) {
				if (rhs.ifa_addr != 0 and rhs.ifa_addr->sa_family == AF_INET) {
					const sysx::ipv4_addr addr(*rhs.ifa_addr);
					const sysx::ipv4_addr netmask(*rhs.ifa_netmask);
					if (
						(flags.test(include_loopback) or addr != sysx::ipv4_addr{127,0,0,1})
						and
						(flags.test(include_global) or netmask != sysx::ipv4_addr{255,255,255,255})
					) {
						func(addr, netmask);
					}
				}
			}
		);
	}

	template<class Addr>
	struct Network {

		typedef Addr addr_type;
		typedef typename addr_type::rep_type rep_type;

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

		constexpr rep_type
		start() const noexcept {
			return (addr_long() & mask_long()) + 1;
		}

		constexpr rep_type
		end() const noexcept {
			return (addr_long() & mask_long()) + (~mask_long());
		}

		bool
		is_loopback() const noexcept {
			return _address == sysx::ipv4_addr{127,0,0,1};
		}

		bool
		is_widearea() const noexcept {
			return _netmask == sysx::ipv4_addr{255,255,255,255};
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Network& rhs) {
			return out << stdx::make_fields("address", rhs._address, "netmask", rhs._netmask);
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

	sysx::ipv4_addr
	get_bind_address(Network_flags flags=0) {

		sysx::ipv4_addr ret;
		std::vector<sysx::ipv4_addr> result;
		for_each_ifaddr(
			[&result] (sysx::ipv4_addr addr, sysx::ipv4_addr network) {
				result.push_back(addr);
			},
			flags
		);

		if (!result.empty()) {
			ret = result.front();
		}

		return ret;
	}

}

#endif // APPS_DISCOVERY_DISCOVERY_HH
