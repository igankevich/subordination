namespace factory {

	typedef ::in_port_t Port;

	namespace components {

		template<char C>
		struct Const_char {
			friend std::ostream&
			operator<<(std::ostream& out, Const_char) {
				return out << C;
			}
			friend std::istream&
			operator>>(std::istream& in, Const_char) {
				if (in.get() != C) in.setstate(std::ios::failbit);
				return in;
			}
		};
	
		template<class Base, class Rep>
		struct Num {
			constexpr Num(): n(0) {}
			constexpr Num(Rep x): n(x) {}
			friend std::ostream&
			operator<<(std::ostream& out, Num rhs) {
				return out << rhs.n;
			}
			friend std::istream&
			operator>>(std::istream& in, Num& rhs) {
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

		typedef Const_char<':'> Colon;
		typedef Const_char<'.'> Dot;
		typedef Const_char<'['> Left_br;
		typedef Const_char<']'> Right_br;
		typedef Num<uint16_t, uint32_t> Hextet;
		typedef Num<uint8_t, uint32_t> Octet;

		union IPv4_addr {
		
			typedef ::in_addr_t addr_type;
			typedef struct ::in_addr in_type;
			typedef uint8_t oct_type;
		
			constexpr
			IPv4_addr() noexcept:
				addr(0) {}

			constexpr explicit
			IPv4_addr(addr_type rhs) noexcept:
				addr(rhs) {}

			constexpr
			IPv4_addr(const IPv4_addr& rhs) noexcept:
				addr(rhs.addr) {}

			constexpr explicit
			IPv4_addr(const in_type& rhs) noexcept:
				addr(rhs.s_addr) {}

			constexpr
			IPv4_addr(oct_type o1, oct_type o2,
				oct_type o3, oct_type o4):
				addr(from_octets(o1, o2, o3, o4)) {}
		
			friend std::ostream&
			operator<<(std::ostream& out, IPv4_addr rhs) {
				return out
					<< ((rhs.addr >> 0)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 8)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 16) & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 24) & UINT32_C(0xff));
			}
		
			friend std::istream&
			operator>>(std::istream& in, IPv4_addr& rhs) {
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

			friend packstream&
			operator<<(packstream& out, const IPv4_addr& rhs) {
				return out << rhs.raw;
			}

			friend packstream&
			operator>>(packstream& in, IPv4_addr& rhs) {
				return in >> rhs.raw;
			}
		
			constexpr operator addr_type() const { return addr; }
			constexpr addr_type rep() const { return addr; }
			constexpr operator const in_type&() const { return this->inaddr; }

			constexpr bool operator==(IPv4_addr rhs) const { return addr == rhs.addr; }
			constexpr bool operator!=(IPv4_addr rhs) const { return addr != rhs.addr; }

			constexpr explicit operator bool() const { return addr != 0; }
			constexpr bool operator !() const { return addr == 0; }
		
		private:

			constexpr static addr_type
			from_octets(oct_type o1, oct_type o2,
				oct_type o3, oct_type o4)
			{
				return
					((o1 << 0)  & UINT32_C(0xff)) |
					((o2 << 8)  & UINT32_C(0xff00)) |
					((o3 << 16) & UINT32_C(0xff0000)) |
					((o4 << 24) & UINT32_C(0xff000000));
			}

			addr_type addr;
			in_type inaddr;
			Bytes<addr_type> raw;
		};

		union IPv6_addr {

			typedef uint16_t Field;
			typedef struct ::in6_addr in6_type;
			typedef struct ::sockaddr_in6 sin6_type;
			typedef std::uint128_t addr_type;
			typedef uint16_t hex_type;

			constexpr
			IPv6_addr() noexcept:
				in_addr6{} {}

			constexpr
			IPv6_addr(const IPv6_addr& rhs) noexcept:
				in_addr6(rhs.in_addr6) {}

			constexpr explicit
			IPv6_addr(addr_type rhs) noexcept:
				addr128(rhs) {}

			constexpr explicit
			IPv6_addr(const sin6_type& rhs) noexcept:
				IPv6_addr(rhs.sin6_addr) {}

			constexpr explicit
			IPv6_addr(const in6_type& rhs) noexcept:
				in_addr6(rhs) {}

			constexpr
			IPv6_addr(hex_type h1, hex_type h2,
				hex_type h3, hex_type h4,
				hex_type h5, hex_type h6,
				hex_type h7, hex_type h8):
				addr128(from_hextets(h1, h2, h3, h4,
				h5, h6, h7, h8)) {}

			constexpr
			operator const in6_type&() const {
				return in_addr6;
			}

			constexpr bool
			operator<(const IPv6_addr& rhs) const {
				return addr128 < rhs.addr128;
			}

			constexpr bool
			operator==(const IPv6_addr& rhs) const {
				return addr128 == rhs.addr128;
			}

			constexpr explicit
			operator bool() const {
				return addr128 != 0;
			}

			constexpr bool
			operator !() const {
				return !operator bool();
			}

			friend std::ostream&
			operator<<(std::ostream& out, const IPv6_addr& rhs) {
				std::ostream::sentry s(out);
				if (s) {
					use_flags f(out, std::ios::hex, std::ios::basefield); 
					std::copy(rhs.begin(), rhs.end()-1,
						std::ostream_iterator<Field>(out, ":"));
					out << *(rhs.end()-1);
				}
				return out;
			}

			friend std::istream&
			operator>>(std::istream& in, IPv6_addr& rhs) {
				std::istream::sentry s(in);
				if (!s) { return in; }
				typedef IPv6_addr::Field Field;
				use_flags f(in, std::ios::hex, std::ios::basefield); 
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
				if (in.fail()) {
					std::fill(rhs.begin(), rhs.end(), 0);
				}
				return in;
			}

			friend packstream&
			operator<<(packstream& out, const IPv6_addr& rhs) {
				return out << rhs.raw;
			}

			friend packstream&
			operator>>(packstream& in, IPv6_addr& rhs) {
				return in >> rhs.raw;
			}

		private:
			constexpr const Field* begin() const { return addr; }
			constexpr const Field* end() const { return addr + num_fields(); }

			Field* begin() { return addr; }
			Field* end() { return addr + num_fields(); }

			static constexpr 
			int num_fields() { return sizeof(addr) / sizeof(Field); }

			constexpr static addr_type
			from_hextets(addr_type h1, addr_type h2,
				addr_type h3, addr_type h4,
				addr_type h5, addr_type h6,
				addr_type h7, addr_type h8)
			{
				return
					((h1 << 0)   & UINT128_C(0xffff)) |
					((h2 << 16)  & UINT128_C(0xffff0000)) |
					((h3 << 32)  & UINT128_C(0xffff00000000)) |
					((h4 << 48)  & UINT128_C(0xffff000000000000)) |
					((h5 << 64)  & UINT128_C(0xffff0000000000000000)) |
					((h6 << 80)  & UINT128_C(0xffff00000000000000000000)) |
					((h7 << 96)  & UINT128_C(0xffff000000000000000000000000)) |
					((h8 << 112) & UINT128_C(0xffff0000000000000000000000000000));
			}

			Field addr[8];
			in6_type in_addr6;
			addr_type addr128;
			Bytes<in6_type> raw;
		};

		union Endpoint {

			typedef struct ::sockaddr sa_type;
			typedef struct ::sockaddr_in sin4_type;
			typedef struct ::sockaddr_in6 sin6_type;
			typedef struct ::in_addr in4_type;
			typedef struct ::in6_addr in6_type;
			typedef ::socklen_t socklen_type;
			typedef ::sa_family_t family_type;
			typedef ::in_addr_t addr4_type;
			typedef ::in_port_t port_type;
			typedef std::uint128_t addr6_type;

			constexpr
			Endpoint() noexcept {}

			constexpr
			Endpoint(const Endpoint& rhs) noexcept:
				_addr6(rhs._addr6) {}

			Endpoint(const char* h, const Port p) { addr(h, p); }

			constexpr
			Endpoint(const IPv4_addr h, const Port p) noexcept:
				_addr6{
				#if HAVE_SOCKADDR_LEN
					0,
				#endif
					AF_INET,
					to_network_format<Port>(p),
					to_network_format<IPv4_addr>(h),
					IN6ADDR_ANY_INIT,
					0
				} {}

			constexpr
			Endpoint(const IPv6_addr& h, const Port p) noexcept:
				_addr6{
				#if HAVE_SOCKADDR_LEN
					0,
				#endif
					AF_INET6,
					to_network_format<Port>(p),
					0, // flowinfo
					to_network_format<IPv6_addr>(h),
					0 // scope
				} {}
				
			constexpr
			Endpoint(const sin4_type& rhs) noexcept:
				_addr4(rhs) {}

			constexpr
			Endpoint(const sin6_type& rhs) noexcept:
				_addr6(rhs) {}

			constexpr
			Endpoint(const sa_type& rhs) noexcept:
				_sockaddr(rhs) {}

			constexpr
			Endpoint(const Endpoint& rhs, const Port newport) noexcept:
				_addr6{
				#if HAVE_SOCKADDR_LEN
					0,
				#endif
					rhs.family(),
					to_network_format<Port>(newport),
					rhs.family() == AF_INET6 ? 0 : rhs._addr6.sin6_flowinfo, // flowinfo or sin_addr
					rhs.family() == AF_INET  ? in6_type{} : rhs._addr6.sin6_addr,
					0 // scope
				} {}

			bool
			operator<(const Endpoint& rhs) const noexcept {
				return family() == AF_INET
					? std::make_tuple(family(), addr4(), port4())
					< std::make_tuple(rhs.family(), rhs.addr4(), rhs.port4())
					: std::make_tuple(family(), addr6(), port6())
					< std::make_tuple(rhs.family(), rhs.addr6(), rhs.port6());
			}

			constexpr bool
			operator==(const Endpoint& rhs) const noexcept {
				return (family() == rhs.family() || family() == 0 || rhs.family() == 0)
					&& (family() == AF_INET
					? addr4() == rhs.addr4() && port4() == rhs.port4()
					: addr6() == rhs.addr6() && port6() == rhs.port6());
			}

			constexpr bool
			operator!=(const Endpoint& rhs) const noexcept {
				return !operator==(rhs);
			}

			constexpr explicit
			operator bool() const noexcept {
				return family() != 0 && (family() == AF_INET
					? static_cast<bool>(addr4())
					: static_cast<bool>(addr6()));
			}

			constexpr bool
			operator !() const noexcept {
				return !operator bool();
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Endpoint& rhs) {
				std::ostream::sentry s(out);
				if (s) {
					if (rhs.family() == AF_INET6) {
						Port port = to_host_format<Port>(rhs.port6());
						std::clog << "Writing host = "
							<< rhs.addr6() << ':' << port << std::endl;
						out << Left_br() << rhs.addr6() << Right_br()
							<< Colon() << port;
					} else {
						Port port = to_host_format<Port>(rhs.port4());
						std::clog << "Writing host = "
							<< rhs.addr4() << ':' << port << std::endl;
						out << rhs.addr4() << Colon() << port;
					}
				}
				return out;
			}

			friend std::istream&
			operator>>(std::istream& in, Endpoint& rhs) {
				std::istream::sentry s(in);
				if (s) {
					IPv4_addr host;
					Port port = 0;
					use_flags f(in);
					in.unsetf(std::ios_base::skipws);
					std::streampos oldg = in.tellg();
					if (in >> host >> Colon() >> port) {
						rhs.addr4(host, port);
						std::clog << "Reading host4 = "
							<< host << ':' << port << std::endl;
					} else {
						in.clear();
						in.seekg(oldg);
						IPv6_addr host6;
						if (in >> Left_br() >> host6 >> Right_br() >> Colon() >> port) {
							rhs.addr6(host6, port);
							std::clog << "Reading host6 = "
								<< host6 << ':' << port << std::endl;
						}
					}
				}
				return in;
			}

			friend packstream&
			operator<<(packstream& out, const Endpoint& rhs) {
				out << rhs.family();
				if (rhs.family() == AF_INET6) {
					out << rhs.addr6() << make_bytes(rhs.port6());
				} else {
					out << rhs.addr4() << make_bytes(rhs.port4());
				}
				return out;
			}

			friend packstream&
			operator>>(packstream& in, Endpoint& rhs) {
				in >> rhs._addr6.sin6_family;
				Bytes<Port> port;
				if (rhs.family() == AF_INET6) {
					IPv6_addr addr;
					in >> addr >> port;
					rhs._addr6.sin6_addr = addr;
					rhs._addr6.sin6_port = port;
				} else {
					IPv4_addr addr;
					in >> addr >> port;
					rhs._addr4.sin_addr = addr;
					rhs._addr4.sin_port = port;
				}
				return in;
			}

			constexpr addr4_type
			address() const noexcept {
				return to_host_format<addr4_type>(this->addr4());
			}

			constexpr Port
			port() const noexcept {
				return to_host_format<Port>(this->port4());
			}

			constexpr family_type
			family() const noexcept {
				return this->_addr6.sin6_family;
			}

		private:
			template<class Q>
			constexpr static
			Q position_helper(Q a, Q netmask) noexcept {
				return a - (a & netmask);
			}

		public:
			constexpr addr4_type
			position(addr4_type netmask) const noexcept {
				return position_helper(address(), netmask);
			}

			inline sa_type*
			sockaddr() noexcept {
				return &this->_sockaddr;
			}

			inline sa_type*
			sockaddr() const noexcept {
				return const_cast<sa_type*>(&this->_sockaddr);
			}

			constexpr socklen_type
			sockaddrlen() const {
				return this->family() == AF_INET6
					? sizeof(sin6_type)
					: sizeof(sin4_type);
			}

		private:

			constexpr IPv4_addr
			addr4() const {
				return IPv4_addr(this->_addr4.sin_addr);
			}

			constexpr Port
			port4() const {
				return this->_addr4.sin_port;
			}

			constexpr IPv6_addr
			addr6() const {
				return IPv6_addr(this->_addr6.sin6_addr);
			}

			constexpr Port
			port6() const {
				return this->_addr6.sin6_port;
			}

			void addr4(IPv4_addr a, Port p) {
				this->_addr4.sin_family = AF_INET;
				this->_addr4.sin_addr.s_addr = a;
				this->_addr4.sin_port = to_network_format<Port>(p);
			}

			void addr6(IPv6_addr a, Port p) {
				this->_addr6.sin6_family = AF_INET6;
				this->_addr6.sin6_addr = a;
				this->_addr6.sin6_port = to_network_format<Port>(p);
			}
		
			void addr(const char* host, Port p) {
				IPv4_addr a4;
				std::stringstream tmp(host);
				if (tmp >> a4) {
					addr4(a4, p);
				} else {
					tmp.clear();
					tmp.seekg(0);
					IPv6_addr a6;
					if (tmp >> a6) {
						addr6(a6, p);
					}
				}
			}

			void addr(const IPv4_addr h, Port p) {
				this->_addr4.sin_family = AF_INET;
				this->_addr4.sin_port = to_network_format<Port>(p);
				this->_addr4.sin_addr.s_addr = to_network_format<IPv4_addr::addr_type>(h);
			}

			void addr(const IPv6_addr& h, Port p) {
				Bytes<IPv6_addr> tmp = h;
				tmp.to_network_format();
				this->_addr6.sin6_family = AF_INET6;
				this->_addr6.sin6_port = to_network_format<Port>(p);
				this->_addr6.sin6_addr = tmp.value();
			}

			sin6_type _addr6 = {};
			sin4_type _addr4;
			sa_type _sockaddr;
		};

		static_assert(sizeof(Endpoint) == sizeof(Endpoint::sin6_type), "bad endpoint size");
		static_assert(sizeof(Endpoint::family_type) == 1, "bad family_type size");
		static_assert(sizeof(Endpoint::port_type) == 2, "bad port_type size");

		template<unsigned int radix, class Ch>
		constexpr Ch to_int(Ch ch) {
			return radix == 16 && ch >= 'a' ? ch-'a'+10 : ch-'0';
		}

		constexpr uint32_t shift_octet(uint32_t octet, uint32_t nshift) {
			return nshift == 0 ? (octet & 0xff) :
				nshift == 8 ? ((octet << nshift) & 0xff00) :
				nshift == 16 ? ((octet << nshift) & 0xff0000) :
				nshift == 24 ? ((octet << nshift) & 0xff000000) :
				0;
		}

		template<uint32_t radix=10, class It>
		constexpr
		uint32_t do_parse_ipv4_addr(It first, It last,
			uint32_t val=0,
			uint32_t octet=0,
			uint32_t nshift=0,
			uint32_t ndots=0) {
			return ndots > 3 || octet > 255 ? 0 :
				first == last ?  (val | shift_octet(octet, nshift)) :
				*first == '.' ?  do_parse_ipv4_addr(first+1, last,
					val | shift_octet(octet, nshift),
					0, nshift + 8, ndots + 1) :
				(*first >= '0' && *first <= '9') ?
				do_parse_ipv4_addr(first+1, last, val,
					octet*radix + to_int<radix>(*first),
					nshift, ndots) :
				0;
		}
	}

	using components::IPv4_addr;
	using components::IPv6_addr;
	using components::Endpoint;

	constexpr IPv4_addr
	operator"" _ipv4(const char* arr, std::size_t n) noexcept {
		return IPv4_addr(components::do_parse_ipv4_addr(arr, arr+n));
	}

}
