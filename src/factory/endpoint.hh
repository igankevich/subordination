#ifdef FACTORY_PACKSTREAM
namespace factory {
	// TODO: this is not portable (now it is even less portable than it used to be...)
	inline
	packstream& operator<<(packstream& out, const Endpoint& rhs) {
		return out << make_bytes(rhs);
	}
	inline
	packstream& operator>>(packstream& in, Endpoint& rhs) {
		Bytes<Endpoint> tmp;
		in.read(tmp.begin(), tmp.size());
		rhs = tmp;
		return in;
	}
	inline
	std::streambuf& operator<<(std::streambuf& buf, const Endpoint& rhs) {
		Bytes<Endpoint> bytes = rhs;
		buf.sputn(bytes.begin(), bytes.size());
		return buf;
	}
	inline
	std::streambuf& operator>>(std::streambuf& buf, Endpoint& rhs) {
		Bytes<Endpoint> bytes;
		buf.sgetn(bytes.begin(), bytes.size());
		rhs = bytes;
		return buf;
	}
}
#else
namespace factory {

	typedef std::string Host;
	typedef ::in_port_t Port;

	namespace components {

		template<char C>
		struct Const_char {
			friend std::ostream& operator<<(std::ostream& out, Const_char) {
				return out << C;
			}
			friend std::istream& operator>>(std::istream& in, Const_char) {
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

		typedef Const_char<':'> Colon;
		typedef Const_char<'.'> Dot;
		typedef Const_char<'['> Left_br;
		typedef Const_char<']'> Right_br;
		typedef Num<uint16_t, uint32_t> Hextet;
		typedef Num<uint8_t, uint32_t> Octet;
	}

	union IPv4_addr {
	
		typedef ::in_addr_t Rep;
		typedef struct ::in_addr Addr;
	
		constexpr IPv4_addr(): addr(0) {}
		constexpr IPv4_addr(Rep a): addr(a) {}
		constexpr IPv4_addr(const IPv4_addr&) = default;
		constexpr IPv4_addr(const struct ::sockaddr_in& a): addr(a.sin_addr.s_addr) {}
		constexpr IPv4_addr(const struct ::in_addr& a): addr(a.s_addr) {}
	
		friend std::ostream& operator<<(std::ostream& out, IPv4_addr rhs) {
			using namespace components;
			return out
				<< ((rhs.addr >> 0)  & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 8)  & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 16) & UINT32_C(0xff)) << Dot()
				<< ((rhs.addr >> 24) & UINT32_C(0xff));
		}
	
		friend std::istream& operator>>(std::istream& in, IPv4_addr& rhs) {
			using namespace components;
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
		constexpr operator const Addr&() const { return this->inaddr; }

		constexpr bool operator==(IPv4_addr rhs) const { return addr == rhs.addr; }
		constexpr bool operator!=(IPv4_addr rhs) const { return addr != rhs.addr; }

		constexpr explicit operator bool() const { return addr != 0; }
		constexpr bool operator !() const { return addr == 0; }
	
	private:
		Rep addr;
		Addr inaddr;
	};

	namespace components {

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

	constexpr IPv4_addr
	operator"" _ipv4(const char* arr, std::size_t n) noexcept {
		using namespace ::factory::components;
		return do_parse_ipv4_addr(arr, arr+n);
	}

	union IPv6_addr {

		typedef uint16_t Field;
		typedef struct ::in6_addr Addr;

		constexpr IPv6_addr(): in_addr6{} {}
		constexpr IPv6_addr(const IPv6_addr&) = default;
		constexpr IPv6_addr(const struct ::sockaddr_in6& a): in_addr6(a.sin6_addr) {}
		constexpr IPv6_addr(const struct ::in6_addr& a): in_addr6(a) {}

		constexpr
		operator const Addr&() const { return in_addr6; }

		inline
		bool operator<(const IPv6_addr& rhs) const {
			return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
		}

		inline
		bool operator==(const IPv6_addr& rhs) const {
			return std::equal(begin(), end(), rhs.begin());
		}

		inline
		explicit operator bool() const {
			return !std::all_of(begin(), end(), [] (Field x) { return x == 0; });
		}

		inline
		bool operator !() const { return !operator bool(); }

		friend std::ostream& operator<<(std::ostream& out, const IPv6_addr& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				typedef std::remove_reference<decltype(rhs)>::type IPv6_addr;
				typedef IPv6_addr::Field Field;
				std::ios_base::fmtflags oldf = out.setf(std::ios::hex);
				std::ostream_iterator<Field> it(out, ":");
				std::copy(rhs.begin(), rhs.end()-1, it);
				out << *(rhs.end()-1);
				out.flags(oldf);
			}
			return out;
		}

		friend std::istream& operator>>(std::istream& in, IPv6_addr& rhs) {
			std::istream::sentry s(in);
			if (!s) { return in; }
			using namespace components;
			typedef std::remove_reference<decltype(rhs)>::type IPv6_addr;
			typedef IPv6_addr::Field Field;
			std::ios_base::fmtflags oldf = in.setf(std::ios::hex);
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
		Addr in_addr6;
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
		typedef std::uint128_t addr6_type;

		constexpr
		Endpoint() noexcept {}

		constexpr
		Endpoint(const Endpoint&) noexcept = default;

		Endpoint(const char* h, const Port p) { addr(h, p); }

		constexpr
		Endpoint(const IPv4_addr h, const Port p) noexcept:
			_addr6{
				AF_INET,
				to_network_format<Port>(p),
				to_network_format<IPv4_addr>(h.rep()),
				IN6ADDR_ANY_INIT,
				0
			} {}

		constexpr
		Endpoint(const IPv6_addr& h, const Port p) noexcept:
			_addr6{
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

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				using namespace components;
				if (rhs.family() == AF_INET6) {
					out << Left_br() << rhs.addr6() << Right_br()
						<< Colon() << to_host_format<Port>(rhs.port6());
				} else {
					out << rhs.addr4() << Colon() << to_host_format<Port>(rhs.port4());
				}
			}
			return out;
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			std::istream::sentry s(in);
			if (s) {
				using namespace components;
				IPv4_addr host;
				Port port;
				std::ios_base::fmtflags oldf = in.flags();
				in >> std::noskipws;
				std::streampos oldg = in.tellg();
				if (in >> host >> Colon() >> port) {
					rhs.addr4(host, port);
					std::clog << "Reading host = " << host << std::endl;
				} else {
					in.clear();
					in.seekg(oldg);
					IPv6_addr host6;
					if (in >> Left_br() >> host6 >> Right_br() >> Colon() >> port) {
						rhs.addr6(host6, port);
						std::clog << "Reading host = " << host6 << std::endl;
					}
				}
				in.flags(oldf);
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
			return this->_addr4.sin_addr;
		}

		constexpr Port
		port4() const {
			return this->_addr4.sin_port;
		}

		constexpr IPv6_addr
		addr6() const {
			return this->_addr6.sin6_addr;
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
			this->_addr4.sin_addr.s_addr = to_network_format<IPv4_addr::Rep>(h);
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

}
#endif
