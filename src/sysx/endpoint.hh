#ifndef SYSX_ENDPOINT_HH
#define SYSX_ENDPOINT_HH

#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>

#include <istream>
#include <sstream>
#include <tuple>


#include <sysx/uint128.hh>
#include <sysx/packetstream.hh>

#include <sysx/bits/check.hh>
#include <sysx/bits/endpoint.hh>
#include <sysx/bits/ifaddrs.hh>
#include <sysx/network_format.hh>
#include <stdx/iosx.hh>


namespace sysx {

	typedef struct ::sockaddr sa_type;
	typedef struct ::sockaddr_in sockinet4_type;
	typedef struct ::sockaddr_in6 sockinet6_type;
	typedef struct ::in_addr inet4_type;
	typedef struct ::in6_addr inet6_type;
	typedef ::socklen_t socklen_type;
	typedef bits::family_type family_type;
	typedef bits::legacy_family_type sa_family_type;
	typedef ::in_addr_t addr4_type;
	typedef std::uint128_t addr6_type;
	typedef ::in_port_t port_type;

	union ipv4_addr {

		typedef uint8_t oct_type;

		constexpr
		ipv4_addr() noexcept:
			addr(0) {}

		constexpr explicit
		ipv4_addr(addr4_type rhs) noexcept:
			addr(rhs) {}

		constexpr
		ipv4_addr(const ipv4_addr& rhs) noexcept:
			addr(rhs.addr) {}

		constexpr explicit
		ipv4_addr(const inet4_type& rhs) noexcept:
			addr(rhs.s_addr) {}

		constexpr
		ipv4_addr(oct_type o1, oct_type o2,
			oct_type o3, oct_type o4):
			addr(from_octets(o1, o2, o3, o4)) {}

		ipv4_addr(const sa_type& rhs) noexcept;

		friend std::ostream&
		operator<<(std::ostream& out, ipv4_addr rhs) {
			using bits::Dot;
			return out
				<< ((rhs.addr >> 0)  & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 8)  & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 16) & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 24) & UINT32_C(0xff));
		}

		friend std::istream&
		operator>>(std::istream& in, ipv4_addr& rhs) {
			using bits::Dot; using bits::Octet;
			Octet o1, o2, o3, o4;
			in >> o1 >> Dot() >> o2 >> Dot() >> o3 >> Dot() >> o4;
			if (!in.fail()) {
				rhs.addr = ipv4_addr::from_octets(o1, o2, o3, o4);
			}
			return in;
		}

		friend packetstream&
		operator<<(packetstream& out, const ipv4_addr& rhs) {
			return out << rhs.raw;
		}

		friend packetstream&
		operator>>(packetstream& in, ipv4_addr& rhs) {
			return in >> rhs.raw;
		}

		constexpr addr4_type rep() const { return addr; }

		constexpr
		operator const inet4_type&() const noexcept {
			return this->inaddr;
		}

		constexpr bool
		operator<(const ipv4_addr& rhs) const noexcept {
			return addr < rhs.addr;
		}

		constexpr bool
		operator==(ipv4_addr rhs) const noexcept {
			return addr == rhs.addr;
		}

		constexpr bool
		operator!=(ipv4_addr rhs) const noexcept {
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
		inet4_type inaddr;
		Bytes<addr4_type> raw;
	};

	constexpr ipv4_addr
	operator"" _ipv4(const char* arr, std::size_t n) noexcept {
		return ipv4_addr(bits::do_parse_ipv4_addr<addr4_type>(arr, arr+n));
	}

	union ipv6_addr {

		typedef uint16_t hex_type;

		constexpr
		ipv6_addr() noexcept:
			inaddr{} {}

		constexpr
		ipv6_addr(const ipv6_addr& rhs) noexcept:
			inaddr(rhs.inaddr) {}

		constexpr explicit
		ipv6_addr(addr6_type rhs) noexcept:
			addr(rhs) {}

		constexpr explicit
		ipv6_addr(const inet6_type& rhs) noexcept:
			inaddr(rhs) {}

		constexpr
		ipv6_addr(hex_type h1, hex_type h2,
			hex_type h3, hex_type h4,
			hex_type h5, hex_type h6,
			hex_type h7, hex_type h8):
			addr(from_hextets(h1, h2, h3, h4,
			h5, h6, h7, h8)) {}

		constexpr
		operator const inet6_type&() const {
			return inaddr;
		}

		constexpr const inet6_type&
		rep() const noexcept {
			return inaddr;
		}

		constexpr bool
		operator<(const ipv6_addr& rhs) const {
			return addr < rhs.addr;
		}

		constexpr bool
		operator==(const ipv6_addr& rhs) const {
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
		operator<<(std::ostream& out, const ipv6_addr& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				typedef ipv6_addr::hex_type hex_type;
				typedef std::ostream::char_type char_type;
				stdx::ios_guard g(out);
				out.setf(std::ios::hex, std::ios::basefield);
				std::copy(rhs.begin(), rhs.end(),
					stdx::intersperse_iterator<hex_type,char_type>(out, ':'));
			}
			return out;
		}

		friend std::istream&
		operator>>(std::istream& in, ipv6_addr& rhs) {
			std::istream::sentry s(in);
			if (!s) { return in; }
			typedef ipv6_addr::hex_type hex_type;
			stdx::ios_guard g(in);
			in.setf(std::ios::hex, std::ios::basefield);
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
				bits::Hextet h = 0;
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
				// we do not need to change byte order here
				// rhs.raw.to_network_format();
			}
			return in;
		}

		friend packetstream&
		operator<<(packetstream& out, const ipv6_addr& rhs) {
			return out << rhs.raw;
		}

		friend packetstream&
		operator>>(packetstream& in, ipv6_addr& rhs) {
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
			using namespace literals;
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
		inet6_type inaddr;
		hex_type hextets[8];
		Bytes<inet6_type> raw;

		static_assert(sizeof(addr) == sizeof(inaddr)
			and sizeof(addr) == sizeof(hextets)
			and sizeof(addr) == sizeof(raw),
			"bad ipv6_addr size");
	};

	template<class Addr>
	constexpr sockinet6_type
	new_sockinet(family_type f, port_type p, Addr h);

	template<>
	constexpr sockinet6_type
	new_sockinet<ipv6_addr>(family_type f, port_type p, ipv6_addr h) {
		return sockinet6_type{
			#ifdef __MACH__
			0,
			#endif
			static_cast<sa_family_type>(f),
			to_network_format<port_type>(p),
			0, // flowinfo
			h.rep(),
			0 // scope
		};
	}

	template<>
	constexpr sockinet6_type
	new_sockinet<ipv4_addr>(family_type f, port_type p, ipv4_addr h) {
		return sockinet6_type{
				#ifdef __MACH__
				0,
				#endif
				static_cast<sa_family_type>(f),
				to_network_format<port_type>(p),
				h.rep(),
				IN6ADDR_ANY_INIT,
				0 // scope
		};
	}

	union endpoint {

		typedef uint16_t portable_family_type;

		constexpr
		endpoint() noexcept {}

		constexpr
		endpoint(const endpoint& rhs) noexcept:
			_addr6(rhs._addr6) {}

		endpoint(const char* h, const port_type p) { addr(h, p); }

		constexpr
		endpoint(const ipv4_addr h, const port_type p) noexcept:
			_addr6(new_sockinet(family_type::inet, p, h)) {}

		constexpr
		endpoint(const ipv6_addr& h, const port_type p) noexcept:
			_addr6(new_sockinet(family_type::inet6, p, h)) {}

		constexpr
		endpoint(const sockinet4_type& rhs) noexcept:
			_addr4(rhs) {}

		constexpr
		endpoint(const sockinet6_type& rhs) noexcept:
			_addr6(rhs) {}

		constexpr
		endpoint(const sa_type& rhs) noexcept:
			_sockaddr(rhs) {}

		constexpr
		endpoint(const endpoint& rhs, const port_type newport) noexcept:
			_addr6(
				rhs.family() == family_type::inet ?
				new_sockinet<ipv4_addr>(family_type::inet, newport,
					ipv4_addr(rhs._addr6.sin6_flowinfo)) :
				new_sockinet<ipv6_addr>(family_type::inet6, newport,
					ipv6_addr(rhs._addr6.sin6_addr))) {}

		bool
		operator<(const endpoint& rhs) const noexcept {
			return family() == family_type::inet
				? std::make_tuple(sa_family(), addr4(), port4())
				< std::make_tuple(rhs.sa_family(), rhs.addr4(), rhs.port4())
				: std::make_tuple(sa_family(), addr6(), port6())
				< std::make_tuple(rhs.sa_family(), rhs.addr6(), rhs.port6());
		}

		constexpr bool
		operator==(const endpoint& rhs) const noexcept {
			return (sa_family() == rhs.sa_family() || sa_family() == 0 || rhs.sa_family() == 0)
				&& (family() == family_type::inet
				? addr4() == rhs.addr4() && port4() == rhs.port4()
				: addr6() == rhs.addr6() && port6() == rhs.port6());
		}

		constexpr bool
		operator!=(const endpoint& rhs) const noexcept {
			return !operator==(rhs);
		}

		constexpr explicit
		operator bool() const noexcept {
			return sa_family() != 0 && (family() == family_type::inet
				? static_cast<bool>(addr4())
				: static_cast<bool>(addr6()));
		}

		constexpr bool
		operator !() const noexcept {
			return !operator bool();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const endpoint& rhs) {
			using bits::Left_br; using bits::Right_br;
			using bits::Colon;
			std::ostream::sentry s(out);
			if (s) {
				if (rhs.family() == family_type::inet6) {
					port_type port = to_host_format<port_type>(rhs.port6());
					out << Left_br() << rhs.addr6() << Right_br()
						<< Colon() << port;
				} else {
					port_type port = to_host_format<port_type>(rhs.port4());
					out << rhs.addr4() << Colon() << port;
				}
			}
			return out;
		}

		friend std::istream&
		operator>>(std::istream& in, endpoint& rhs) {
			using bits::Left_br; using bits::Right_br;
			using bits::Colon;
			std::istream::sentry s(in);
			if (s) {
				ipv4_addr host;
				port_type port = 0;
				stdx::ios_guard g(in);
				in.unsetf(std::ios_base::skipws);
				std::streampos oldg = in.tellg();
				if (in >> host >> Colon() >> port) {
					rhs.addr4(host, port);
				} else {
					in.clear();
					in.seekg(oldg);
					ipv6_addr host6;
					if (in >> Left_br() >> host6 >> Right_br() >> Colon() >> port) {
						rhs.addr6(host6, port);
					}
				}
			}
			return in;
		}

		friend packetstream&
		operator<<(packetstream& out, const endpoint& rhs) {
			out << rhs.family();
			if (rhs.family() == family_type::inet6) {
				out << rhs.addr6() << make_bytes(rhs.port6());
			} else {
				out << rhs.addr4() << make_bytes(rhs.port4());
			}
			return out;
		}

		friend packetstream&
		operator>>(packetstream& in, endpoint& rhs) {
			family_type fam;
			in >> fam;
			rhs._addr6.sin6_family = static_cast<sa_family_type>(fam);
			Bytes<port_type> port;
			if (rhs.family() == family_type::inet6) {
				ipv6_addr addr;
				in >> addr >> port;
				rhs._addr6.sin6_addr = addr;
				rhs._addr6.sin6_port = port;
			} else {
				ipv4_addr addr;
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

		constexpr port_type
		port() const noexcept {
			return to_host_format<port_type>(this->port4());
		}

		constexpr family_type
		family() const noexcept {
			return static_cast<family_type>(this->_addr6.sin6_family);
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
			return this->family() == family_type::inet6
				? sizeof(sockinet6_type)
				: sizeof(sockinet4_type);
		}

	private:

		constexpr sa_family_type
		sa_family() const noexcept {
			return this->_addr6.sin6_family;
		}

		constexpr ipv4_addr
		addr4() const {
			return ipv4_addr(this->_addr4.sin_addr);
		}

		constexpr port_type
		port4() const {
			return this->_addr4.sin_port;
		}

		constexpr ipv6_addr
		addr6() const {
			return ipv6_addr(this->_addr6.sin6_addr);
		}

		constexpr port_type
		port6() const {
			return this->_addr6.sin6_port;
		}

		void addr4(ipv4_addr a, port_type p) {
			this->_addr4.sin_family = static_cast<sa_family_type>(family_type::inet);
			this->_addr4.sin_addr = a;
			this->_addr4.sin_port = to_network_format<port_type>(p);
		}

		void addr6(ipv6_addr a, port_type p) {
			this->_addr6.sin6_family = static_cast<sa_family_type>(family_type::inet6);
			this->_addr6.sin6_addr = a;
			this->_addr6.sin6_port = to_network_format<port_type>(p);
		}

		void addr(const char* host, port_type p) {
			ipv4_addr a4;
			std::stringstream tmp(host);
			if (tmp >> a4) {
				addr4(a4, p);
			} else {
				tmp.clear();
				tmp.seekg(0);
				ipv6_addr a6;
				if (tmp >> a6) {
					addr6(a6, p);
				}
			}
		}

		void addr(const ipv4_addr h, port_type p) {
			this->_addr4.sin_family = static_cast<sa_family_type>(family_type::inet);
			this->_addr4.sin_port = to_network_format<port_type>(p);
			this->_addr4.sin_addr = h;
		}

		void addr(const ipv6_addr& h, port_type p) {
			this->_addr6.sin6_family = static_cast<sa_family_type>(family_type::inet6);
			this->_addr6.sin6_port = to_network_format<port_type>(p);
			this->_addr6.sin6_addr = h;
		}

		sockinet6_type _addr6 = {};
		sockinet4_type _addr4;
		sa_type _sockaddr;

		friend ipv4_addr;
	};

	static_assert(sizeof(endpoint) == sizeof(sockinet6_type), "bad endpoint size");
	static_assert(sizeof(port_type) == 2, "bad port_type size");

	ipv4_addr::ipv4_addr(const sa_type& rhs) noexcept:
	inaddr(sysx::endpoint(rhs)._addr4.sin_addr)
	{}

	struct ifaddrs {

		typedef struct ::ifaddrs ifaddrs_type;
		typedef ifaddrs_type value_type;
		typedef bits::ifaddrs_iterator<ifaddrs_type> iterator;
		typedef std::size_t size_type;

		ifaddrs() {
			bits::check(::getifaddrs(&this->_addrs),
				__FILE__, __LINE__, __func__);
		}
		~ifaddrs() noexcept {
			if (this->_addrs) {
				::freeifaddrs(this->_addrs);
			}
		}

		iterator
		begin() noexcept {
			return iterator(this->_addrs);
		}

		iterator
		begin() const noexcept {
			return iterator(this->_addrs);
		}

		static constexpr iterator
		end() noexcept {
			return iterator();
		}

		bool
		empty() const noexcept {
			return this->begin() == this->end();
		}

		size_type
		size() const noexcept {
			return std::distance(this->begin(), this->end());
		}

	private:
		ifaddrs_type* _addrs = nullptr;
	};

}

#endif // SYSX_ENDPOINT_HH
