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
		
			friend std::istream& operator>>(std::istream& in, Interval<I>& rhs) {
				return in >> rhs._start >> rhs._end;
			}
		
			bool empty() const { return _start >= _end; }
			I count() const { return _end - _start; }
		
		private:
			I _start, _end;
		};
		
		typedef Interval<uint32_t> Address_range;
		
		union Address {
		
			explicit Address(uint32_t a=0): _addr(a) {}
		
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
		
		std::vector<Address_range> discover_neighbours();

		inline
		unix::endpoint get_bind_address() {

			unix::endpoint ret;
		
			Ifaddrs addrs;
			Ifaddrs::iterator result = 
			std::find_if(addrs.begin(), addrs.end(), [] (const Ifaddrs::ifaddrs_type& rhs) {

				if (rhs.ifa_addr == NULL || rhs.ifa_addr->sa_family != AF_INET) {
					// ignore non-internet networks
					return false;
				}

				unix::endpoint addr(*rhs.ifa_addr);
				if (addr.address() == unix::endpoint("127.0.0.1", 0).address()) {
					// ignore localhost and non-IPv4 addresses
					return false;
				}

				unix::endpoint netmask(*rhs.ifa_netmask);
				if (netmask.address() == unix::endpoint("255.255.255.255",0).address()) {
					// ignore wide-area networks
					return false;
				}
		
				return true;
			});
			if (result != addrs.end()) {
				ret = unix::endpoint(*result->ifa_addr);
			}
		
			return ret;
		}

	}

}
