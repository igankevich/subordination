#ifndef APPS_DISCOVERY_DISCOVERY_HH
#define APPS_DISCOVERY_DISCOVERY_HH

#include <set>
#include <map>
#include <vector>
#include <chrono>

#include <stdx/log.hh>

#include <sysx/bits/check.hh>
#include <sysx/endpoint.hh>

namespace factory {

	namespace components {

		template<class I>
		inline
		I log2(I x) {
			I n = 0;
			while (x >>= 1) n++;
			return n;
		}

		inline
		uint32_t log(uint32_t x, uint32_t p = 1) {
			uint32_t n = 0;
			while (x >>= p) n++;
			return n;
		}

		template<class I>
		struct Interval {

			typedef I int_type;

			constexpr
			Interval() noexcept:
			_start(0), _end(0) {}

			constexpr
			Interval(I a, I b) noexcept:
			_start(a), _end(b) {}

			constexpr
			Interval(const Interval& rhs) noexcept:
			_start(rhs._start), _end(rhs._end) {}

			constexpr
			I start() const noexcept {
				return _start;
			}

			constexpr
			I end() const noexcept {
				return _end;
			}

			constexpr bool
			overlaps(const Interval& rhs) const noexcept {
				return (_start < rhs._start && _end > rhs._start)
					|| (rhs._start < _start && rhs._end > _start);
			}

			inline Interval&
			operator+=(const Interval& rhs) noexcept {
				_start = std::min(_start, rhs._start);
				_end = std::max(_end, rhs._end);
				return *this;
			}

			constexpr bool
			operator<(const Interval& rhs) const noexcept {
				return _start < rhs._start;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Interval& rhs) {
				return out << rhs._start << ' ' << rhs._end;
			}

			friend std::istream&
			operator>>(std::istream& in, Interval& rhs) {
				return in >> rhs._start >> rhs._end;
			}

			constexpr bool
			empty() const noexcept {
				return !(_start < _end);
			}

			constexpr I
			count() const noexcept {
				return _end - _start;
			}

		private:
			I _start, _end;
		};

		typedef Interval<uint32_t> Address_range;

		std::vector<Address_range> discover_neighbours() {

			struct ::ifaddrs* ifaddr;
			sysx::bits::check(::getifaddrs(&ifaddr),
				__FILE__, __LINE__, __func__);

			std::set<Address_range> ranges;

			for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

				if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
					// ignore non-internet networks
					continue;
				}

				sysx::endpoint addr(*ifa->ifa_addr);
				if (addr.address() == sysx::endpoint("127.0.0.1", 0).address()) {
					// ignore localhost and non-IPv4 addresses
					continue;
				}

				sysx::endpoint netmask(*ifa->ifa_netmask);
				if (netmask.address() == sysx::endpoint("255.255.255.255",0).address()) {
					// ignore wide-area networks
					continue;
				}

				uint32_t addr_long = addr.address();
				uint32_t mask_long = netmask.address();

				uint32_t start = (addr_long & mask_long) + 1;
				uint32_t end = (addr_long & mask_long) + (~mask_long);

				ranges.insert(Address_range(start, addr_long));
				ranges.insert(Address_range(addr_long+1, end));
			}

			// combine overlaping ranges
			std::vector<Address_range> sorted_ranges;
			Address_range prev_range;
			std::for_each(ranges.cbegin(), ranges.cend(),
				[&sorted_ranges, &prev_range](const Address_range& range)
			{
				if (prev_range.empty()) {
					prev_range = range;
				} else {
					if (prev_range.overlaps(range)) {
						prev_range += range;
					} else {
						sorted_ranges.push_back(prev_range);
						prev_range = range;
					}
				}
			});

			if (!prev_range.empty()) {
				sorted_ranges.push_back(prev_range);
			}

			std::for_each(sorted_ranges.cbegin(), sorted_ranges.cend(),
				[] (const Address_range& range)
			{
				std::clog << sysx::ipv4_addr(range.start()) << '-' << sysx::ipv4_addr(range.end()) << '\n';
			});

			::freeifaddrs(ifaddr);

			return sorted_ranges;
		}

		enum Network_flags_t {
			include_loopback = 1 << 0,
			include_local    = 1 << 1,
			include_global   = 1 << 2,
			exclude_right    = 1 << 3,
			exclude_left     = 1 << 4
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

		std::vector<Interval<sysx::addr4_type>>
		optimise_ranges(const std::set<Interval<sysx::addr4_type>>& ranges) {
			// combine overlaping ranges
			std::vector<Interval<sysx::addr4_type>> sorted_ranges;
			Interval<sysx::addr4_type> prev_range;
			std::for_each(
				ranges.cbegin(), ranges.cend(),
				[&sorted_ranges, &prev_range](const Interval<sysx::addr4_type>& range) {
					if (prev_range.empty()) {
						prev_range = range;
					} else {
						if (prev_range.overlaps(range)) {
							prev_range += range;
						} else {
							sorted_ranges.push_back(prev_range);
							prev_range = range;
						}
					}
				}
			);

			if (!prev_range.empty()) {
				sorted_ranges.push_back(prev_range);
			}

			return sorted_ranges;
		}

		std::vector<Interval<sysx::addr4_type>>
		enumerate_networks(Network_flags flags=0) {
			std::set<Interval<sysx::addr4_type>> ranges;
			for_each_ifaddr(
				[&ranges,flags] (sysx::ipv4_addr addr, sysx::ipv4_addr netmask) {
					const uint32_t addr_long = sysx::to_host_format(addr.rep());
					const uint32_t mask_long = sysx::to_host_format(netmask.rep());

					const uint32_t start = (addr_long & mask_long) + 1;
					const uint32_t end = (addr_long & mask_long) + (~mask_long);

					if (!flags.test(exclude_left)) ranges.insert(Interval<sysx::addr4_type>(start, addr_long));
					if (!flags.test(exclude_right)) ranges.insert(Interval<sysx::addr4_type>(addr_long+1, end));
				},
				flags
			);
			return optimise_ranges(ranges);
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

}

#endif // APPS_DISCOVERY_DISCOVERY_HH
