#ifndef APPS_DISCOVERY_DISCOVERY_HH
#define APPS_DISCOVERY_DISCOVERY_HH

#include <set>
#include <map>
#include <vector>
#include <chrono>
#include <cassert>
#include <algorithm>

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
		struct Interval_set;

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
				assert(overlaps(rhs));
				merge(rhs);
				return *this;
			}

			void
			merge(const Interval& rhs) noexcept {
				_start = std::min(_start, rhs._start);
				_end = std::max(_end, rhs._end);
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

		template<class I>
		struct Interval_set {

			typedef Interval<I> interval_type;

			void
			insert(const interval_type& rhs) noexcept {
				auto result = std::find_if(
					_set.begin(), _set.end(),
					[&rhs] (const interval_type& x) {
						return x.overlaps(rhs);
					}
				);
				if (result == _set.end()) {
					_set.insert(rhs);
				} else {
					interval_type acc = rhs;
					auto first = result;
					auto last = _set.end();
					while (first != last && acc.overlaps(*first)) {
						acc.merge(*first);
					}
					const auto it = _set.erase(result, first);
					_set.insert(it, acc);
				}
			}

			template<class Function>
			void
			for_each(Function func) {
				typedef typename interval_type::int_type int_type;
				std::for_each(_set.begin(), _set.end(),
					[&func] (const interval_type& rhs) {
						for (int_type i=rhs.start(); i<rhs.end(); ++i) {
							func(i);
						}
					}
				);
			}

			template<class Set>
			void
			flatten(Set& rhs) const noexcept {
				typedef typename interval_type::int_type int_type;
				this->for_each(
					[&rhs] (int_type i) {
						rhs.insert(i);
					}
				);
			}

		private:

			std::set<interval_type> _set;
		};

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

		Interval_set<sysx::addr4_type>
		enumerate_networks(Network_flags flags=0) {
			Interval_set<sysx::addr4_type> ranges;
			for_each_ifaddr(
				[&ranges,flags] (sysx::ipv4_addr addr, sysx::ipv4_addr netmask) {
					const sysx::addr4_type addr_long = sysx::to_host_format(addr.rep());
					const sysx::addr4_type mask_long = sysx::to_host_format(netmask.rep());

					const sysx::addr4_type start = (addr_long & mask_long) + 1;
					const sysx::addr4_type end = (addr_long & mask_long) + (~mask_long);

					if (!flags.test(exclude_left)) ranges.insert(Interval<sysx::addr4_type>(start, addr_long));
					if (!flags.test(exclude_right)) ranges.insert(Interval<sysx::addr4_type>(addr_long+1, end));
				},
				flags
			);
			return ranges;
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
