namespace factory {

	typedef std::string Host;
	typedef uint16_t Port;


	struct Endpoint {

		Endpoint(): _host(), _port(0) {}
		Endpoint(const Host& host, Port port): _host(inet_address(host.c_str())), _port(port) {}
		Endpoint(const Endpoint& rhs): _host(rhs._host), _port(rhs._port) {}
		Endpoint(struct ::sockaddr_in* addr) {
			char str_address[40];
			::inet_ntop(AF_INET, &addr->sin_addr.s_addr, str_address, 40);
			_host = str_address;
			_port = ntohs(addr->sin_port);
		}

		Endpoint& operator=(const Endpoint& rhs) {
			_host = rhs._host;
			_port = rhs._port;
			return *this;
		}

		bool operator<(const Endpoint& rhs) const {
			return address() < rhs.address();
		}

		bool operator==(const Endpoint& rhs) const {
			return host() == rhs.host() && port() == rhs.port();
		}

		bool operator!=(const Endpoint& rhs) const {
			return host() != rhs.host() || port() != rhs.port();
		}

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			return out << rhs.host() << ':' << rhs.port();
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			std::getline(in, rhs._host, ':');
			return in >> rhs._port;
		}

//		friend Foreign_stream& operator<<(Foreign_stream& out, const Endpoint& rhs) {
//			return out << rhs._host << rhs._port;
//		}
//
//		friend Foreign_stream& operator>>(Foreign_stream& in, Endpoint& rhs) {
//			return in >> rhs._host >> rhs._port;
//		}

		const Host& host() const { return _host; }
		Port port() const { return _port; }

		std::string address() const {
			std::stringstream addr;
			addr << *this;
			return addr.str();
		}

		void to_sockaddr(struct ::sockaddr_in* addr) const {
			std::memset(addr, 0, sizeof(sockaddr_in));
			addr->sin_family = AF_INET;
			addr->sin_port = htons(_port);
			check("inet_pton()", ::inet_pton(AF_INET, _host.c_str(), &addr->sin_addr.s_addr));
		}

	private:

		Host inet_address(const char* host) {
			struct ::addrinfo* info;
			check("getaddrinfo", ::getaddrinfo(host, NULL, NULL, &info));
			char str_address[40];
			::inet_ntop(AF_INET, &((struct ::sockaddr_in*)info->ai_addr)->sin_addr.s_addr, str_address, 40);
			Host numeric_host = str_address;
			::freeaddrinfo(info);
			return numeric_host;
		}

		Host _host;
		Port _port;
	};

}
