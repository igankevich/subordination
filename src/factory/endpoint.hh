namespace factory {

	typedef std::string Host;
	typedef uint16_t Port;

	struct Endpoint {

		Endpoint(): _host(), _port(0) {}
		Endpoint(const Host& host, Port port): _host(host), _port(port) {}
		Endpoint(const Endpoint& rhs): _host(rhs._host), _port(rhs._port) {}

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

	private:
		Host _host;
		Port _port;
	};

}
