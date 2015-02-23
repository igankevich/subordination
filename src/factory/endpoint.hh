namespace factory {

	typedef std::string Host;
	typedef uint16_t Port;


	struct Endpoint {

		Endpoint() { std::memset((void*)&_addr, 0, sizeof(_addr)); }
		Endpoint(const Host& host, Port port) { addr(host.c_str(), port); }
		Endpoint(const Endpoint& rhs) { addr(&rhs._addr); }
		Endpoint(struct ::sockaddr_in* rhs) { addr(rhs); }

		Endpoint& operator=(const Endpoint& rhs) {
			addr(&rhs._addr);
			return *this;
		}

		bool operator<(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr < rhs._addr.sin_addr.s_addr;
		}

		bool operator==(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr == rhs._addr.sin_addr.s_addr
				&& _addr.sin_port == rhs._addr.sin_port;
		}

		bool operator!=(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr != rhs._addr.sin_addr.s_addr
				|| _addr.sin_port != rhs._addr.sin_port;
		}

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			char host[64];
			check_inet("inet_ntop()", ::inet_ntop(AF_INET, &rhs._addr.sin_addr.s_addr, host, sizeof(host)));
			return out << host << ':' << ntohs(rhs._addr.sin_port);
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			std::string host;
			std::getline(in, host, ':');
			Port port;
			in >> port;
			rhs.addr(host.c_str(), port);
			return in;
		}

//		friend Foreign_stream& operator<<(Foreign_stream& out, const Endpoint& rhs) {
//			return out << rhs._host << rhs._port;
//		}
//
//		friend Foreign_stream& operator>>(Foreign_stream& in, Endpoint& rhs) {
//			return in >> rhs._host >> rhs._port;
//		}

//		const Host& host() const { return _host; }
		Port port() const { return ntohs(_addr.sin_port); }
		struct ::sockaddr_in* addr() { return &_addr; }

//		std::string address() const {
//			std::stringstream addr;
//			addr << *this;
//			return addr.str();
//		}

//		void to_sockaddr(struct ::sockaddr_in* addr) const {
//			std::memset(addr, 0, sizeof(sockaddr_in));
//			addr->sin_family = AF_INET;
//			addr->sin_port = htons(_port);
//			check("inet_pton()", ::inet_pton(AF_INET, _host.c_str(), &addr->sin_addr.s_addr));
//		}

	private:
	
		void addr(const struct ::sockaddr_in* rhs) {
			std::memcpy((void*)&_addr, (const void*)rhs, sizeof(_addr));
		}

		void addr(const char* host, Port port) {
			struct ::addrinfo* info;
			check("getaddrinfo", ::getaddrinfo(host, NULL, NULL, &info));
			struct ::sockaddr_in* tmp = (struct ::sockaddr_in*) info->ai_addr;
			tmp->sin_port = htons(port);
			addr(tmp);
			::freeaddrinfo(info);
		}

		struct ::sockaddr_in _addr;
	};

}
