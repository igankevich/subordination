#include <ifaddrs.h>
#include <set>

namespace factory {

	template<class I>
	struct Interval {
	
		Interval(): _start(0), _end(0) {}
		Interval(I a, I b): _start(a), _end(b) {}
		Interval(const Interval<I>& rhs): _start(rhs._start), _end(rhs._end) {}
	
		I start() const { return _start; }
		I end() const { return _end; }
	
		bool overlaps(const Interval<I>& rhs) const {
			return (_start < rhs._start && _end > rhs._start)
				|| (rhs._start < _start && rhs._end > _start);
		}
	
		Interval<I>& operator+=(const Interval<I>& rhs) {
			_start = std::min(_start, rhs._start);
			_end = std::max(_end, rhs._end);
			return *this;
		}
		
		bool operator<(const Interval<I>& rhs) const {
			return _start < rhs._start;
		}
	
		friend std::ostream& operator<<(std::ostream& out, const Interval<I>& rhs) {
			return out << rhs._start << ' ' << rhs._end;
		}
	
		bool empty() const { return _start >= _end; }
		I count() const { return _end - _start; }
	
	private:
		I _start, _end;
	};
	
	typedef Interval<uint32_t> Address_range;
	
	union Address {
	
		explicit Address(uint32_t a): _addr(a) {}
	
		friend std::ostream& operator<<(std::ostream& out, const Address& rhs) {
			return out
				<< int(rhs._bytes[3]) << '.'
				<< int(rhs._bytes[2]) << '.'
				<< int(rhs._bytes[1]) << '.'
				<< int(rhs._bytes[0]);
		}
	
	private:
		uint32_t _addr;
		unsigned char _bytes[sizeof(_addr)];
	};
	
	std::vector<Address_range> discover_neighbours() {
	
		struct ::ifaddrs* ifaddr;
		check("getifaddrs()", ::getifaddrs(&ifaddr));
	
		std::set<Address_range> ranges;
	
		for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	
			// ignore localhost and non-IPv4 addresses
			if (ifa->ifa_addr == NULL
				|| htonl(((struct ::sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr) == 0x7f000001ul
				|| ifa->ifa_addr->sa_family != AF_INET)
			{
				continue;
			}
	
			uint32_t addr_long = htonl(((struct ::sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr);
			uint32_t mask_long = htonl(((struct ::sockaddr_in*) ifa->ifa_netmask)->sin_addr.s_addr);
	
			// ignore 255.255.255.255 wide-area networks
			if (mask_long == std::numeric_limits<uint32_t>::max()) {
				continue;
			}
	
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
			std::clog << Address(range.start()) << '-' << Address(range.end()) << '\n';
		});
	
		::freeifaddrs(ifaddr);
	
		return sorted_ranges;
	}

	struct Broadcast_sink {

		Broadcast_sink(Socket s, Port p):
			_socket(s), _dest(discover_neighbours()), _port(p) {}

		ssize_t write(const char* buf, size_t size) {
			int n = _dest.size();
			ssize_t ret = 0;
			for (int i=0; i<n; ++i) {
				uint32_t start = _dest[i].start();
				uint32_t end = _dest[i].end();
				for (uint32_t j=start; j<end; ++j) {
					struct ::sockaddr_in addr;
					std::memset(&addr, 0, sizeof(addr));
					addr.sin_family = AF_INET;
					addr.sin_port = htons(_port);
					addr.sin_addr.s_addr = ntohl(j);
//					std::clog << "addr = " << Address(j) << std::endl;
					ret |= ::sendto(_socket, buf, size, 0, (struct ::sockaddr*)&addr, sizeof(addr));
				}
			}
			return ret;
		}
	
	private:
		Socket _socket;
		std::vector<Address_range> _dest;
		Port _port;
	};

	struct Broadcast_source {

		explicit Broadcast_source(Socket s): _socket(s), _from() {}

		ssize_t read(char* buf, size_t size) {
			struct ::sockaddr_in addr;
			std::memset(&addr, 0, sizeof(addr));
			socklen_t addr_len = sizeof(addr);
			ssize_t ret = ::recvfrom(_socket, buf, size, 0, (struct ::sockaddr*)&addr, &addr_len);
			if (ret != -1) {
				_from = Endpoint(&addr);
			}
			return ret;
		}

		const Endpoint& from() const { return _from; }

	private:
		Socket _socket;
		Endpoint _from;
	};

	namespace components {

		/// Rough description of a protocol.
		/// >> { rating_1, 0 }
		/// << { rating_2, 0 }
		/// >> { rating_1/ttl_1, 1 }
		/// << { rating_2/ttl_2, 1 }
		/// Node 1 has discovered a node with rating rating_2/ttl_2.
		/// Node 2 has discovered a node with rating rating_1/ttl_1.

		template<class Kernel>
		struct Discovery_kernel: public Kernel {

			typedef uint32_t Rating;

			Discovery_kernel(): _endpoint(), _rating(0) {}

			void act() {
				std::cout << "Discovered " << _endpoint << ", rating = " << _rating << std::endl;
			}

			void write(Foreign_stream& out) { _rating = 123; out << _rating; }

			void read(Foreign_stream& in) { in >> _rating; }

			Endpoint from() const { return _endpoint; }
			void from(Endpoint e) { _endpoint = e; }

		private:
			Endpoint _endpoint;
			Rating _rating;
		};

	}

}
