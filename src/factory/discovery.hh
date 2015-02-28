#include <ifaddrs.h>
#include <set>

namespace factory {

	template<class I>
	struct Interval {

		typedef I Int;
	
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

	Endpoint random_endpoint(const std::vector<Address_range>& addrs, Port port) {

		typedef typename Address_range::Int I;

		int n = addrs.size();
		I total_count = 0;
		for (int i=0; i<n; ++i) {
			total_count += addrs[i].end() - addrs[i].start();
		}

		static std::default_random_engine generator(std::chrono::steady_clock::now().time_since_epoch().count());
		std::uniform_int_distribution<I> distribution(0, total_count-1);
		I m = distribution(generator);

		int i = 0;
		I cnt;
		while (m > (cnt = addrs[i].end() - addrs[i].start()) && i < n) {
			m -= cnt;
			i++;
		}

		I addr = addrs[i].start() + m;
		return Endpoint(addr, port);
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
		/// >> { 1, time_1 }
		/// << { 2, time_1, time_2 }
		/// >> { 3, time_2, r1=rating_1/(cur_time_1-time_1) }
		/// << { 4, r2=rating_2/(cur_time_2-time_2) }
		/// Node 1 has discovered a node with rating r2. If r1>r2, node 1 becomes a leader,
		/// Node 2 has discovered a node with rating r1. If r2>r1, node 2 becomes a leader,


		template<class K>
		struct Remote: public K {

			Remote(): _endpoint() {}
			Endpoint from() const { return _endpoint; }
			void from(Endpoint e) { _endpoint = e; }

		private:
			Endpoint _endpoint;
		};

		// TODO: auto-deletion of kernels
		// TODO: auto-deletion of sockets from server 
		template<template<class X> class Mobile, class Type>
		struct Discovery_kernel: public Remote<Mobile<Discovery_kernel<Mobile, Type>>> {

			typedef uint32_t Rating;
			typedef uint64_t Time;

			enum State { 
				TIME_1 = 1,
				TIME_2 = 2,
				TIME_AND_RATING_1 = 3,
				TIME_AND_RATING_2 = 4
			};

			Discovery_kernel():
				_time_1(0),
				_time_2(0),
				_rating_1(0),
				_rating_2(0)
			{}

			void act() {

				debug_recv();
				State old_state = _state;

				switch (_state) {
					case TIME_1: _state = TIME_2; break;
					case TIME_2: _state = TIME_AND_RATING_1; break;
					case TIME_AND_RATING_1: _state = TIME_AND_RATING_2; break;
					case TIME_AND_RATING_2: break;
				}

				if (old_state == TIME_AND_RATING_2) {
					// delete when final state is reached
					delete this;
				} else {
					discovery_send(this, this->from());
				}
			}

			void write(Foreign_stream& out) {
				switch (_state) {
					case TIME_1: set_time_1(); out << char(_state) << _time_1; break;
					case TIME_2: set_time_2(); out << char(_state) << _time_1 << _time_2; break;
					case TIME_AND_RATING_1: out << char(_state) << _time_2 << _rating_1; break;
					case TIME_AND_RATING_2: out << char(_state) << _rating_2; break;
				}
				// delete after write is done since there are no downstream objects
				delete this;
			}

			void read(Foreign_stream& in) {
				uint8_t st = 0;
				in >> st;
				_state = State(st);
				switch (_state) {
					case TIME_1: in >> _time_1; break;
					case TIME_2: in >> _time_1 >> _time_2; set_rating_1(); break;
					case TIME_AND_RATING_1: in >> _time_2 >> _rating_1; set_rating_2(); break;
					case TIME_AND_RATING_2: in >> _rating_2; break;
				}
			}

			static void init_type(Type* t) { t->id(1); }

		private:

			void debug_recv() const {
				std::cerr
					<< "Discovery {"
					<< _state << ", "
					<< _time_1 << ", "
					<< _time_2 << ", "
					<< _rating_1 << ", "
					<< _rating_2 << "} from "
					<< this->from()
					<< std::endl;
			}

			void debug_send() const {
				std::cerr
					<< "Discovery {"
					<< _state << ", "
					<< _time_1 << ", "
					<< _time_2 << ", "
					<< _rating_1 << ", "
					<< _rating_2 << "} to "
					<< this->from()
					<< std::endl;
			}

			void set_time_1() { _time_1 = current_time(); }
			void set_time_2() { _time_2 = current_time(); }
			Time dt(Time t0) const { return std::max(current_time()-t0, Time(1)); }

			void set_rating_1() {
				_rating_1 = 123000000/dt(_time_1);
			}
			void set_rating_2() { _rating_2 = 456; }

			static Time current_time() {
				return std::chrono::steady_clock::now().time_since_epoch().count();
			}

			State _state = TIME_1;
			Time _time_1;
			Time _time_2;
			Rating _rating_1;
			Rating _rating_2;
		};

	}

}
