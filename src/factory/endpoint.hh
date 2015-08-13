#ifndef FACTORY_ENDPOINT_HH
#define FACTORY_ENDPOINT_HH

#include "bits/endpoint.hh"

namespace factory {

	typedef ::in_port_t Port;

	namespace components {

		typedef struct ::sockaddr sa_type;
		typedef struct ::sockaddr_in sin4_type;
		typedef struct ::sockaddr_in6 sin6_type;
		typedef struct ::in_addr in4_type;
		typedef struct ::in6_addr in6_type;
		typedef ::socklen_t socklen_type;
		typedef ::sa_family_t family_type;
		typedef ::in_addr_t addr4_type;
		typedef std::uint128_t addr6_type;
		typedef ::in_port_t port_type;

		union IPv4_addr {
		
			typedef uint8_t oct_type;
		
			constexpr
			IPv4_addr() noexcept:
				addr(0) {}

			constexpr explicit
			IPv4_addr(addr4_type rhs) noexcept:
				addr(rhs) {}

			constexpr
			IPv4_addr(const IPv4_addr& rhs) noexcept:
				addr(rhs.addr) {}

			constexpr explicit
			IPv4_addr(const in4_type& rhs) noexcept:
				addr(rhs.s_addr) {}

			constexpr
			IPv4_addr(oct_type o1, oct_type o2,
				oct_type o3, oct_type o4):
				addr(from_octets(o1, o2, o3, o4)) {}
		
			friend std::ostream&
			operator<<(std::ostream& out, IPv4_addr rhs) {
				using bits::Dot;
				return out
					<< ((rhs.addr >> 0)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 8)  & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 16) & UINT32_C(0xff)) << Dot()
					<< ((rhs.addr >> 24) & UINT32_C(0xff));
			}
		
			friend std::istream&
			operator>>(std::istream& in, IPv4_addr& rhs) {
				using bits::Dot; using bits::Octet;
				Octet o1, o2, o3, o4;
				in >> o1 >> Dot() >> o2 >> Dot() >> o3 >> Dot() >> o4;
				if (!in.fail()) {
					rhs.addr = IPv4_addr::from_octets(o1, o2, o3, o4);
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
		
			constexpr addr4_type rep() const { return addr; }

			constexpr
			operator const in4_type&() const noexcept {
				return this->inaddr;
			}

			constexpr bool
			operator<(const IPv4_addr& rhs) const noexcept {
				return addr < rhs.addr;
			}

			constexpr bool
			operator==(IPv4_addr rhs) const noexcept {
				return addr == rhs.addr;
			}

			constexpr bool
			operator!=(IPv4_addr rhs) const noexcept {
				return addr != rhs.addr;
			}

			constexpr explicit
			operator bool() const noexcept {
				return addr != 0;
			}

			constexpr bool
			operator !() const noexcept {
				return addr == 0;
			}
		
		private:

			constexpr static addr4_type
			from_octets(oct_type o1, oct_type o2,
				oct_type o3, oct_type o4)
			{
				return (
					((o1 << 0)  & UINT32_C(0xff)) |
					((o2 << 8)  & UINT32_C(0xff00)) |
					((o3 << 16) & UINT32_C(0xff0000)) |
					((o4 << 24) & UINT32_C(0xff000000)));
			}

			addr4_type addr;
			in4_type inaddr;
			Bytes<addr4_type> raw;
		};

		constexpr IPv4_addr
		operator"" _ipv4(const char* arr, std::size_t n) noexcept {
			using components::addr4_type;
			return IPv4_addr(to_network_format(
				bits::do_parse_ipv4_addr<addr4_type>(arr, arr+n)));
		}

		union IPv6_addr {

			typedef uint16_t hex_type;

			constexpr
			IPv6_addr() noexcept:
				inaddr{} {}

			constexpr
			IPv6_addr(const IPv6_addr& rhs) noexcept:
				inaddr(rhs.inaddr) {}

			constexpr explicit
			IPv6_addr(addr6_type rhs) noexcept:
				addr(rhs) {}

			constexpr explicit
			IPv6_addr(const in6_type& rhs) noexcept:
				inaddr(rhs) {}

			constexpr
			IPv6_addr(hex_type h1, hex_type h2,
				hex_type h3, hex_type h4,
				hex_type h5, hex_type h6,
				hex_type h7, hex_type h8):
				addr(from_hextets(h1, h2, h3, h4,
				h5, h6, h7, h8)) {}

			constexpr
			operator const in6_type&() const {
				return inaddr;
			}

			constexpr bool
			operator<(const IPv6_addr& rhs) const {
				return addr < rhs.addr;
			}

			constexpr bool
			operator==(const IPv6_addr& rhs) const {
				return addr == rhs.addr;
			}

			constexpr explicit
			operator bool() const {
				return addr != 0;
			}

			constexpr bool
			operator !() const {
				return !operator bool();
			}

			friend std::ostream&
			operator<<(std::ostream& out, const IPv6_addr& rhs) {
				std::ostream::sentry s(out);
				if (s) {
//					IPv6_addr tmp = rhs;
//					tmp.raw.to_host_format();
					typedef IPv6_addr::hex_type hex_type;
					use_flags f(out, std::ios::hex, std::ios::basefield); 
					std::copy(rhs.begin(), rhs.end(),
						intersperse_iterator<hex_type>(out, ":"));
				}
				return out;
			}

			friend std::istream&
			operator>>(std::istream& in, IPv6_addr& rhs) {
				std::istream::sentry s(in);
				if (!s) { return in; }
				typedef IPv6_addr::hex_type hex_type;
				use_flags f(in, std::ios::hex, std::ios::basefield); 
				int field_no = 0;
				int zeros_field = -1;
				std::for_each(rhs.begin(), rhs.end(),
					[&field_no,&in,&zeros_field,&rhs] (hex_type& field)
				{
					if (in.fail()) return;
					// compressed notation
					if (in.peek() == ':') {
						in.get();
						if (field_no == 0) { in >> bits::Colon(); }
						if (zeros_field != -1) {
							in.setstate(std::ios::failbit);
						} else {
							zeros_field = field_no;
						}
					}
					bits::Hextet h;
					if (in >> h) {
						char ch = in.peek();
						// if prefixed with ::ffff:
						if (field_no >= 1 && rhs.hextets[0] == 0xffff && zeros_field == 0) {
							in >> bits::Dot();
						} else {
							in >> bits::Colon();
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
				} else {
//					rhs.raw.to_network_format();
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
			constexpr const hex_type* begin() const { return hextets; }
			constexpr const hex_type* end() const { return hextets + num_fields(); }

			hex_type* begin() { return hextets; }
			hex_type* end() { return hextets + num_fields(); }

			static constexpr 
			int num_fields() { return sizeof(hextets) / sizeof(hex_type); }

			constexpr static addr6_type
			from_hextets(addr6_type h1, addr6_type h2,
				addr6_type h3, addr6_type h4,
				addr6_type h5, addr6_type h6,
				addr6_type h7, addr6_type h8)
			{
				return (
					((h1 << 0)   & UINT128_C(0xffff)) |
					((h2 << 16)  & UINT128_C(0xffff0000)) |
					((h3 << 32)  & UINT128_C(0xffff00000000)) |
					((h4 << 48)  & UINT128_C(0xffff000000000000)) |
					((h5 << 64)  & UINT128_C(0xffff0000000000000000)) |
					((h6 << 80)  & UINT128_C(0xffff00000000000000000000)) |
					((h7 << 96)  & UINT128_C(0xffff000000000000000000000000)) |
					((h8 << 112) & UINT128_C(0xffff0000000000000000000000000000)));
			}

			addr6_type addr;
			in6_type inaddr;
			hex_type hextets[8];
			Bytes<in6_type> raw;
		};

		union Endpoint {

			typedef uint16_t portable_family_type;

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
					h.rep(),
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
					h,
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
				using bits::Left_br; using bits::Right_br;
				using bits::Colon;
				std::ostream::sentry s(out);
				if (s) {
					if (rhs.family() == AF_INET6) {
						Port port = to_host_format<Port>(rhs.port6());
//						std::clog << "Writing host = "
//							<< rhs.addr6() << ':' << port << std::endl;
						out << Left_br() << rhs.addr6() << Right_br()
							<< Colon() << port;
					} else {
						Port port = to_host_format<Port>(rhs.port4());
//						std::clog << "Writing host = "
//							<< rhs.addr4() << ':' << port << std::endl;
						out << rhs.addr4() << Colon() << port;
					}
				}
				return out;
			}

			friend std::istream&
			operator>>(std::istream& in, Endpoint& rhs) {
				using bits::Left_br; using bits::Right_br;
				using bits::Colon;
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
				typedef Endpoint::portable_family_type portable_family_type;
				out << portable_family_type(rhs.family());
				if (rhs.family() == AF_INET6) {
					out << rhs.addr6() << make_bytes(rhs.port6());
				} else {
					out << rhs.addr4() << make_bytes(rhs.port4());
				}
				return out;
			}

			friend packstream&
			operator>>(packstream& in, Endpoint& rhs) {
				typedef Endpoint::portable_family_type portable_family_type;
				portable_family_type fam;
				in >> fam;
				rhs._addr6.sin6_family = fam;
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
				return to_host_format<addr4_type>(this->addr4().rep());
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
				this->_addr4.sin_addr = a;
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
				this->_addr4.sin_addr = h;
			}

			void addr(const IPv6_addr& h, Port p) {
				this->_addr6.sin6_family = AF_INET6;
				this->_addr6.sin6_port = to_network_format<Port>(p);
				this->_addr6.sin6_addr = h;
			}

			sin6_type _addr6 = {};
			sin4_type _addr4;
			sa_type _sockaddr;
		};

		static_assert(sizeof(Endpoint) == sizeof(sin6_type), "bad endpoint size");
//		static_assert(sizeof(family_type) == 1, "bad family_type size");
		static_assert(sizeof(port_type) == 2, "bad port_type size");

	}

	using components::IPv4_addr;
	using components::IPv6_addr;
	using components::Endpoint;
	using components::operator""_ipv4;

}
#endif // FACTORY_ENDPOINT_HH
