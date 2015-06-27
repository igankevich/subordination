#ifdef FACTORY_FOREIGN_STREAM
namespace factory {
	// TODO: this is not portable
	Foreign_stream& operator<<(Foreign_stream& out, const Endpoint& rhs) {
		return out << rhs.addr()->sin_addr.s_addr << rhs.addr()->sin_port;
	}
	Foreign_stream& operator>>(Foreign_stream& in, Endpoint& rhs) {
		rhs.addr()->sin_family = AF_INET;
		return in >> rhs.addr()->sin_addr.s_addr >> rhs.addr()->sin_port;
	}
}
#else
namespace factory {

	typedef std::string Host;
	typedef uint16_t Port;

	union Endpoint {

		template<char C>
		struct Const_char {
			friend std::ostream& operator<<(std::ostream& out, Const_char rhs) {
				return out << C;
			}
			friend std::istream& operator>>(std::istream& in, Const_char rhs) {
				if (in.get() != C) in.setstate(std::ios::failbit);
				return in;
			}
		};

		typedef struct ::sockaddr Addr;
		typedef struct ::sockaddr_in Addr_in;
		typedef Const_char<':'> Colon;

		struct IPv4_addr {
	
			typedef uint32_t Rep;
			typedef Const_char<'.'> Dot;
	
			struct Octet {
				constexpr Octet(): n(0) {}
				constexpr Octet(Rep x): n(x) {}
				friend std::ostream& operator<<(std::ostream& out, Octet rhs) {
					return out << rhs.n;
				}
				friend std::istream& operator>>(std::istream& in, Octet& rhs) {
					in >> rhs.n;
					if (rhs.n > std::numeric_limits<uint8_t>::max()) {
						in.setstate(std::ios::failbit);
					}
					return in;
				}
				constexpr operator Rep() const { return n; }
				constexpr Rep rep() const { return n; }
			private:
				Rep n;
			};
	
			constexpr IPv4_addr(): addr(0) {}
			constexpr IPv4_addr(Rep a): addr(a) {}
			constexpr IPv4_addr(const Addr_in& a): addr(a.sin_addr.s_addr) {}
	
			friend std::ostream& operator<<(std::ostream& out, IPv4_addr rhs) {
				return out
					<< ((rhs.addr >> 0)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 8)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 16) & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 24) & UINT32_C(0xff));
			}
	
			friend std::istream& operator>>(std::istream& in, IPv4_addr& rhs) {
				Octet o1, o2, o3, o4;
				in >> o1 >> Dot() >> o2 >> Dot() >> o3 >> Dot() >> o4;
				if (!in.fail()) {
					rhs.addr = 
						((o1.rep() << 0)  & UINT32_C(0xff)) |
						((o2.rep() << 8)  & UINT32_C(0xff00)) |
						((o3.rep() << 16) & UINT32_C(0xff0000)) |
						((o4.rep() << 24) & UINT32_C(0xff000000));
				}
				return in;
			}
	
			constexpr operator Rep() const { return addr; }
			constexpr Rep rep() const { return addr; }
	
		private:
			Rep addr;
		};


		constexpr Endpoint() {}
		Endpoint(const char* h, Port p) { addr(h, p); }
		Endpoint(const Host& h, Port p) { addr(h.c_str(), p); }
		Endpoint(uint32_t h, Port p) { addr(h, p); }
		Endpoint(const Endpoint& rhs): _sockaddr(rhs._sockaddr) {}
		Endpoint(Addr_in* rhs): _addr(*rhs) {}
		Endpoint(Addr* rhs): _sockaddr(*rhs) {}

		Endpoint& operator=(const Endpoint& rhs) {
			_sockaddr = rhs._sockaddr;
			return *this;
		}

		constexpr bool operator<(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr < rhs._addr.sin_addr.s_addr
				|| (_addr.sin_addr.s_addr == rhs._addr.sin_addr.s_addr
				&& _addr.sin_port < rhs._addr.sin_port);
		}

		constexpr bool operator==(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr == rhs._addr.sin_addr.s_addr
				&& _addr.sin_port == rhs._addr.sin_port;
		}

		constexpr bool operator!=(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr != rhs._addr.sin_addr.s_addr
				|| _addr.sin_port != rhs._addr.sin_port;
		}

		constexpr explicit operator bool() const { return _addr.sin_addr.s_addr != 0; }
		constexpr bool operator !() const { return _addr.sin_addr.s_addr == 0; }

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			return out << Endpoint::IPv4_addr(rhs._addr)
				<< Endpoint::Colon() << to_host_format<Port>(rhs._addr.sin_port);
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			Endpoint::IPv4_addr host;
			Port port;
			std::ios_base::fmtflags oldf = in.flags();
			if (in >> std::ws >> std::noskipws >> host >> Endpoint::Colon() >> port) {
				rhs._addr.sin_family = AF_INET;
				rhs._addr.sin_addr.s_addr = host;
				rhs._addr.sin_port = to_network_format<Port>(port);
			}
			in.flags(oldf);
			return in;
		}

//		Host host() const {
//			std::ostringstream tmp;
//			tmp << IPv4_addr(_addr);
//			return tmp.str();
//		}

		constexpr uint32_t address() const { return to_host_format<uint32_t>(_addr.sin_addr.s_addr); }
		constexpr Port port() const { return to_host_format<Port>(_addr.sin_port); }
		void port(Port rhs) { _addr.sin_port = to_network_format<Port>(rhs); }

		constexpr uint32_t position(uint32_t netmask) const {
			return position_helper(address(), netmask);
		}

		Addr* sockaddr() { return &_sockaddr; }
		Addr_in* addr() { return &_addr; }
		constexpr const Addr_in* addr() const { return &_addr; }

	private:

		constexpr static
		uint32_t position_helper(uint32_t a, uint32_t netmask) {
			return a - (a & netmask);
		}
	
		void addr(const char* host, Port p) {
			IPv4_addr a;
			std::istringstream tmp(host);
			tmp >> a;
			_addr.sin_family = AF_INET;
			_addr.sin_port = to_network_format<Port>(p);
			_addr.sin_addr.s_addr = a;
		}

		void addr(const uint32_t h, Port p) {
			_addr.sin_family = AF_INET;
			_addr.sin_addr.s_addr = to_network_format<uint32_t>(h);
			_addr.sin_port = to_network_format<Port>(p);
		}

		Addr_in _addr = {};
		Addr _sockaddr;
	};

}
#endif
