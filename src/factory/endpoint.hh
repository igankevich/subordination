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

	std::istream& getline_no_white_space(std::istream& in, std::string& str, char delim) {
		char ch;
		while ((ch = static_cast<char>(in.get())) != std::char_traits<char>::eof()
			&& !std::isspace(ch) && ch != delim)
		{
			str.push_back(ch);
		}
		if (ch != delim) {
			in.putback(ch);
		}
		return in;
	}

	typedef std::string Host;
	typedef uint16_t Port;

	union Endpoint {

		typedef struct ::sockaddr Addr;
		typedef struct ::sockaddr_in Addr_in;

//		Endpoint() { std::memset(static_cast<void*>(&_addr), 0, sizeof(_addr)); }
		constexpr Endpoint(): _addr{0} {}
		Endpoint(const Host& h, Port p) { addr(h.c_str(), p); }
		Endpoint(uint32_t h, Port p) { addr(h, p); }
		Endpoint(const Endpoint& rhs) { addr(&rhs._addr); }
		Endpoint(Addr_in* rhs) { addr(rhs); }
		Endpoint(Addr* rhs) { addr(rhs); }

		Endpoint& operator=(const Endpoint& rhs) {
			addr(&rhs._addr);
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
			char host[64];
			check<const char*>("inet_ntop()",
				::inet_ntop(AF_INET, &rhs._addr.sin_addr.s_addr, host, sizeof(host)),
				nullptr);
			return out << host << ':' << to_host_format<Port>(rhs._addr.sin_port);
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			std::string host;
			Port port;
			in >> std::ws;
			getline_no_white_space(in, host, ':');
			std::ios_base::fmtflags oldf = in.flags() & std::ios::skipws;
			in >> std::noskipws >> port;
			in.flags(oldf);
			if (in) {
				try {
					rhs.addr(host.c_str(), port);
				} catch (...) {
					in.setstate(std::ios_base::failbit);
				}
			}
			return in;
		}

		Host host() const {
			char h[64];
			check<const char*>("inet_ntop()",
				::inet_ntop(AF_INET, &_addr.sin_addr.s_addr, h, sizeof(h)),
				nullptr);
			return Host(h);
		}

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
	
		void addr(const Addr_in* rhs) {
			std::memcpy(static_cast<void*>(&_addr),
				static_cast<const void*>(rhs), sizeof(_addr));
		}

		void addr(const Addr* rhs) {
			_sockaddr = *rhs;
		}

		void addr(const char* h, Port p) {
			_addr.sin_family = AF_INET;
			_addr.sin_port = to_network_format<Port>(p);
			int ret = ::inet_pton(AF_INET, h, &_addr.sin_addr);
			if (ret == 0 || ret == -1) {
				throw ret;
//				std::stringstream msg;
//				msg << "inet_pton(). Bad address: '" << h << "'.";
//				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}

		void addr(const uint32_t h, Port p) {
			Addr_in a;
			a.sin_family = AF_INET;
			a.sin_addr.s_addr = to_network_format<uint32_t>(h);
			a.sin_port = to_network_format<Port>(p);
			addr(&a);
		}

		Addr_in _addr;
		Addr _sockaddr;
	};

}
#endif
