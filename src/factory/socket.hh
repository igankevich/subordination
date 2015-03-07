namespace factory {

	struct Socket {

		typedef int Flag;
		typedef int Option;

		static const int DEFAULT_FLAGS = SOCK_NONBLOCK | SOCK_CLOEXEC;

		Socket(): _socket(create_socket()) {}
		Socket(int socket): _socket(socket) {}
		Socket(const Socket& rhs): _socket(rhs._socket) {}

		int create_socket() {
			return check("socket()", ::socket(AF_INET, SOCK_STREAM | DEFAULT_FLAGS, 0));
		}

		void bind(Endpoint e) {
//			check("socket()", _socket = ::socket(AF_INET, type | DEFAULT_FLAGS, 0));
			options(SO_REUSEADDR);
			check("bind()", ::bind(_socket, (struct sockaddr*)e.addr(), sizeof(sockaddr_in)));
			factory_log(Level::COMPONENT) << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(_socket, SOMAXCONN));
			factory_log(Level::COMPONENT) << "Listening on " << name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
//				check("socket()", _socket = ::socket(AF_INET, type | DEFAULT_FLAGS, 0));
				check_connect("connect()", ::connect(_socket, (struct ::sockaddr*)e.addr(), sizeof(sockaddr_in)));
				factory_log(Level::COMPONENT) << "Connecting to " << e << std::endl;
			} catch (std::system_error& err) {
				factory_log(Level::COMPONENT) << "Rethrowing connection error." << std::endl;
				throw Connection_error(err.what(), __FILE__, __LINE__, __func__);
			}
		}

		// Does not store client's address.
		std::pair<Socket, Endpoint> accept() {
			struct sockaddr_in addr;
			socklen_t acc_len = sizeof(addr);
			Socket socket = check("accept()", ::accept(_socket, (struct sockaddr*)&addr, &acc_len));
			socket.flags(O_NONBLOCK);
			Endpoint endpoint(&addr);
			factory_log(Level::COMPONENT) << "Accepted connection from " << endpoint << std::endl;
			return std::make_pair(socket, endpoint);
		}

		void close() {
			if (_socket >= 0) {
				factory_log(Level::COMPONENT) << "Closing socket " << _socket << std::endl;
				::shutdown(_socket, SHUT_RDWR);
				::close(_socket);
			}
			_socket = -1;
		}

		void flags(Flag f) { ::fcntl(_socket, F_SETFL, flags() | f); }
		Flag flags() const { return ::fcntl(_socket, F_GETFL); }

		void options(Option option) {
			int one = 1;
			check("setsockopt()", ::setsockopt(_socket, SOL_SOCKET, option, &one, sizeof(one)));
		}

		int error() const {
			int ret = 0;
			if (_socket < 0) {
				ret = -1;
			} else {
				socklen_t sz = sizeof(ret);
				factory_log(Level::COMPONENT) << "Socket = " << _socket << std::endl;
				check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, &ret, &sz));
				factory_log(Level::COMPONENT) << "getsockopt(): " << ::strerror(ret) << std::endl;
			}
			// If one connects to localhost to a different port and the service is offline
			// then socket's local port can be chosed to be the same as the port of the service.
			// If this happens the socket connects to itself and sends and replies to
			// its own messages. This conditional solves the issue.
			try {
				if (ret == 0 && name() == peer_name()) {
					ret = -1;
				}
			} catch (std::system_error& err) {
				ret = -1;
			}
			return ret;
		}

		Endpoint name() const {
			struct ::sockaddr_in addr;
			socklen_t len = sizeof(addr);
			check("getsockname()", ::getsockname(_socket, (struct ::sockaddr*)&addr, &len));
			return Endpoint(&addr);
		}

		Endpoint peer_name() const {
			struct ::sockaddr_in addr;
			socklen_t len = sizeof(addr);
			check("getsockname()", ::getpeername(_socket, (struct ::sockaddr*)&addr, &len));
			return Endpoint(&addr);
		}

		ssize_t read(char* buf, size_t size) {
			return ::read(_socket, buf, size);
		}

		ssize_t write(const char* buf, size_t size) {
			return ::write(_socket, buf, size);
		}

		operator int() const { return _socket; }
		bool operator==(const Socket& rhs) const { return _socket == rhs._socket; }

		Socket& operator=(const Socket& rhs) {
			this->close();
			_socket = rhs._socket;
			return *this;
		}

		friend std::ostream& operator<<(std::ostream& out, const Socket& rhs) {
			return out << rhs._socket;
		}

	private:

		static int check_connect(const char* func, int ret) {
			return (errno == EINPROGRESS) ? ret : check(func, ret);
		}

		int _socket;
	};

}
