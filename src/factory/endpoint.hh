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
	
		template<class Base, class Rep>
		struct Num {
			constexpr Num(): n(0) {}
			constexpr Num(Rep x): n(x) {}
			friend std::ostream& operator<<(std::ostream& out, Num rhs) {
				return out << rhs.n;
			}
			friend std::istream& operator>>(std::istream& in, Num& rhs) {
				in >> rhs.n;
				if (rhs.n > std::numeric_limits<Base>::max()) {
					in.setstate(std::ios::failbit);
				}
				return in;
			}
			constexpr operator Rep() const { return n; }
			constexpr Rep rep() const { return n; }
		private:
			Rep n;
		};

		typedef struct ::sockaddr Addr;
		typedef struct ::sockaddr_in Addr4;
		typedef struct ::sockaddr_in6 Addr6;
		typedef struct ::sockaddr_storage Storage;
		typedef ::socklen_t Sock_len;
		typedef ::sa_family_t Family;
		typedef Const_char<':'> Colon;
		typedef Const_char<'.'> Dot;
		typedef Const_char<'['> Left_br;
		typedef Const_char<']'> Right_br;

		struct IPv4_addr {
	
			typedef uint32_t Rep;
			typedef Num<uint8_t, Rep> Octet;
	
			constexpr IPv4_addr(): addr(0) {}
			constexpr IPv4_addr(Rep a): addr(a) {}
			constexpr IPv4_addr(const Addr4& a): addr(a.sin_addr.s_addr) {}
	
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

		union IPv6_addr {

			typedef uint16_t Field;
			typedef struct ::in6_addr In_addr6;

			constexpr IPv6_addr(): addr{} {}
			constexpr IPv6_addr(const Addr6& a): inaddr6(a.sin6_addr) {}

			operator const In_addr6&() const { return inaddr6; }

			bool operator<(const IPv6_addr& rhs) const {
				return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
			}

			bool operator==(const IPv6_addr& rhs) const {
				return std::equal(begin(), end(), rhs.begin());
			}

			operator bool() const {
				return !std::all_of(begin(), end(), [] (Field x) { return x == 0; });
			}

			bool operator !() const { return !operator bool(); }

			friend std::ostream& operator<<(std::ostream& out, const IPv6_addr& rhs) {
				typedef std::remove_reference<decltype(rhs)>::type IPv6_addr;
				typedef IPv6_addr::Field Field;
				std::ios_base::fmtflags oldf = out.flags();
				out << std::hex;
				std::ostream_iterator<Field> it(out, ":");
				std::copy(rhs.begin(), rhs.end()-1, it);
				out << *(rhs.end()-1);
				out.flags(oldf);
				return out;
			}

			friend std::istream& operator>>(std::istream& in, IPv6_addr& rhs) {
				typedef Endpoint::Num<uint16_t, uint32_t> Hextet;
				typedef Endpoint::Colon Colon;
				typedef Endpoint::Dot Dot;
				typedef std::remove_reference<decltype(rhs)>::type IPv6_addr;
				typedef IPv6_addr::Field Field;
				std::ios_base::fmtflags oldf = in.flags();
				in >> std::hex;
				int field_no = 0;
				int zeros_field = -1;
				std::for_each(rhs.begin(), rhs.end(), [&field_no,&in,&zeros_field,&rhs] (Field& field) {
					if (in.fail()) return;
					// compressed notation
					if (in.peek() == ':') {
						in.get();
						if (field_no == 0) { in >> Colon(); }
						if (zeros_field != -1) {
							in.setstate(std::ios::failbit);
						} else {
							zeros_field = field_no;
						}
					}
					Hextet h;
					if (in >> h) {
						char ch = in.peek();
						// if prefixed with ::ffff:
						if (field_no >= 1 && rhs.addr[0] == 0xffff && zeros_field == 0) {
							in >> Dot();
						} else {
							in >> Colon();
						}
						// put back the first character after the address
						if (in.fail()) {
							in.clear();
							in.putback(ch);
						}
						field = h;
						++field_no;
					}
				});
				// push fields after :: towards the end 
				if (zeros_field != -1) {
					in.clear();
					auto zeros_start = rhs.begin() + zeros_field;
					auto zeros_end = rhs.end() - (field_no - zeros_field);
					std::copy(zeros_start, rhs.begin() + field_no, zeros_end);
					std::fill(zeros_start, zeros_end, 0);
				}
				in.flags(oldf);
				if (in.fail()) {
					std::fill(rhs.begin(), rhs.end(), 0);
				}
				return in;
			}

		private:
			constexpr const Field* begin() const { return addr; }
			constexpr const Field* end() const { return addr + num_fields(); }

			Field* begin() { return addr; }
			Field* end() { return addr + num_fields(); }

			static constexpr 
			int num_fields() { return sizeof(addr) / sizeof(Field); }

			Field addr[8];
			In_addr6 inaddr6;
		};

		constexpr Endpoint() {}
		Endpoint(const char* h, Port p) { addr(h, p); }
		Endpoint(const Host& h, Port p) { addr(h.c_str(), p); }
		Endpoint(IPv4_addr h, Port p) { addr(h, p); }
		Endpoint(const IPv6_addr& h, Port p) { addr(h, p); }
		constexpr Endpoint(const Endpoint& rhs): storage(rhs.storage) {}
		constexpr Endpoint(const Addr4& rhs): addr4(rhs) {}
		constexpr Endpoint(const Addr6& rhs): addr6(rhs) {}
		constexpr Endpoint(const Addr& rhs): _sockaddr(rhs) {}

		Endpoint& operator=(const Endpoint& rhs) {
			storage = rhs.storage;
			return *this;
		}

		constexpr bool operator<(const Endpoint& rhs) const {
			return family() == AF_INET
				? std::make_pair(addr4.sin_addr.s_addr, addr4.sin_port) <
				std::make_pair(rhs.addr4.sin_addr.s_addr, rhs.addr4.sin_port)
				: std::make_pair(IPv6_addr(addr6), addr6.sin6_port) <
				std::make_pair(IPv6_addr(rhs.addr6), rhs.addr6.sin6_port);
		}

		constexpr bool operator==(const Endpoint& rhs) const {
			return (family() == rhs.family() || family() == 0 || rhs.family() == 0)
				&& (family() == AF_INET
				? addr4.sin_addr.s_addr == rhs.addr4.sin_addr.s_addr
				&& addr4.sin_port == rhs.addr4.sin_port
				: IPv6_addr(addr6) == IPv6_addr(rhs.addr6)
				&& addr6.sin6_port == rhs.addr6.sin6_port);
		}

		constexpr bool operator!=(const Endpoint& rhs) const {
			return !operator==(rhs);
		}

		constexpr explicit operator bool() const {
			return family() != 0 && (family() == AF_INET ? addr4.sin_addr.s_addr != 0 : IPv6_addr(addr6));
		}

		constexpr bool operator !() const { return !operator bool(); }

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			if (rhs.family() == AF_INET6) {
				out << Endpoint::Left_br() << Endpoint::IPv6_addr(rhs.addr6)
					<< Endpoint::Right_br();
			} else {
				out << Endpoint::IPv4_addr(rhs.addr4);
			}
			out << Endpoint::Colon() << to_host_format<Port>(rhs.addr4.sin_port);
			return out;
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			Endpoint::IPv4_addr host;
			Endpoint::IPv6_addr host6;
			Port port;
			std::ios_base::fmtflags oldf = in.flags();
			in >> std::ws >> std::noskipws;
			std::streampos oldg = in.tellg();
			if (in >> host >> Endpoint::Colon() >> port) {
				rhs.addr4.sin_family = AF_INET;
				rhs.addr4.sin_addr.s_addr = host;
				rhs.addr4.sin_port = to_network_format<Port>(port);
				std::clog << "Reading host = " << host << std::endl;
			} else {
				in.clear();
				in.seekg(oldg);
				if (in >> Endpoint::Left_br() >> host6 >> Endpoint::Right_br() >> Endpoint::Colon() >> port) {
					rhs.addr6.sin6_family = AF_INET6;
					rhs.addr6.sin6_addr = host6;
					rhs.addr6.sin6_port = to_network_format<Port>(port);
					std::clog << "Reading host = " << host6 << std::endl;
				}
			}
			in.flags(oldf);
			return in;
		}

//		Host host() const {
//			std::ostringstream tmp;
//			tmp << IPv4_addr(addr4);
//			return tmp.str();
//		}

		constexpr uint32_t address() const { return to_host_format<uint32_t>(addr4.sin_addr.s_addr); }
		constexpr Port port() const { return to_host_format<Port>(addr4.sin_port); }
		void port(Port rhs) { addr4.sin_port = to_network_format<Port>(rhs); }

		constexpr uint32_t position(uint32_t netmask) const {
			return position_helper(address(), netmask);
		}

		Addr* sockaddr() { return &_sockaddr; }
		constexpr Sock_len sockaddrlen() const {
			return family() == AF_INET6 ? sizeof(Addr6) : sizeof(Addr4);
		}
		Addr4* addr() { return &addr4; }
		constexpr const Addr4* addr() const { return &addr4; }

		constexpr Family family() const { return storage.ss_family; }

	private:

		constexpr static
		uint32_t position_helper(uint32_t a, uint32_t netmask) {
			return a - (a & netmask);
		}
	
		void addr(const char* host, Port p) {
			IPv4_addr a4;
			std::stringstream tmp(host);
			if (tmp >> a4) {
				addr4.sin_family = AF_INET;
				addr4.sin_addr.s_addr = a4;
				addr4.sin_port = to_network_format<Port>(p);
			} else {
				tmp.clear();
				tmp.seekg(0);
				IPv6_addr a6;
				if (tmp >> a6) {
					addr6.sin6_family = AF_INET6;
					addr6.sin6_addr = a6;
					addr6.sin6_port = to_network_format<Port>(p);
				}
			}
		}

		void addr(const IPv4_addr h, Port p) {
			addr4.sin_family = AF_INET;
			addr4.sin_addr.s_addr = to_network_format<IPv4_addr::Rep>(h);
			addr4.sin_port = to_network_format<Port>(p);
		}

		void addr(const IPv6_addr& h, Port p) {
			Bytes<IPv6_addr> tmp = h;
			tmp.to_network_format();
			addr6.sin6_family = AF_INET6;
			addr6.sin6_addr = tmp.value();
			addr6.sin6_port = to_network_format<Port>(p);
		}

		Addr6 addr6 = {};
		Addr4 addr4;
		Addr _sockaddr;
		Storage storage;
	};

	static_assert(sizeof(Endpoint) == sizeof(Endpoint::Storage), "Bad sockaddr_storage size");

}
#endif
