#ifndef DISCOVERY_DISCOVERY_HH
#define DISCOVERY_DISCOVERY_HH

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
		sysx::endpoint get_bind_address() {

			sysx::endpoint ret;

			sysx::ifaddrs addrs;
			sysx::ifaddrs::iterator result =
			std::find_if(addrs.begin(), addrs.end(), [] (const sysx::ifaddrs::ifaddrs_type& rhs) {

				if (rhs.ifa_addr == NULL || rhs.ifa_addr->sa_family != AF_INET) {
					// ignore non-internet networks
					return false;
				}

				sysx::endpoint addr(*rhs.ifa_addr);
				if (addr.address() == sysx::endpoint("127.0.0.1", 0).address()) {
					// ignore localhost and non-IPv4 addresses
					return false;
				}

				sysx::endpoint netmask(*rhs.ifa_netmask);
				if (netmask.address() == sysx::endpoint("255.255.255.255",0).address()) {
					// ignore wide-area networks
					return false;
				}

				return true;
			});
			if (result != addrs.end()) {
				ret = sysx::endpoint(*result->ifa_addr);
			}

			return ret;
		}

	}

}

#endif // DISCOVERY_DISCOVERY_HH
