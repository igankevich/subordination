namespace factory {

	struct Socket {

		typedef int Flag;
		typedef int Option;

		static const int DEFAULT_FLAGS = SOCK_NONBLOCK | SOCK_CLOEXEC;

		Socket(): _socket(0) {}
		Socket(int socket): _socket(socket) {}
		Socket(const Socket& rhs): _socket(rhs._socket) {}
		Socket(Socket&& rhs): _socket(rhs._socket) { rhs._socket = -1; }

		void create_socket_if_necessary() {
			if (_socket <= 0) {
				check("socket()", _socket = ::socket(AF_INET, SOCK_STREAM | DEFAULT_FLAGS, 0));
				linger();
			}
		}

		void bind(Endpoint e) {
			create_socket_if_necessary();
			options(SO_REUSEADDR);
			check("bind()", ::bind(_socket, (struct sockaddr*)e.addr(), sizeof(sockaddr_in)));
			Logger(Level::COMPONENT) << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(_socket, SOMAXCONN));
			Logger(Level::COMPONENT) << "Listening on " << name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
				create_socket_if_necessary();
				check_connect("connect()", ::connect(_socket, (struct ::sockaddr*)e.addr(), sizeof(sockaddr_in)));
				Logger(Level::COMPONENT) << "Connecting to " << e << std::endl;
			} catch (std::system_error& err) {
				Logger(Level::COMPONENT) << "Rethrowing connection error." << std::endl;
				throw Connection_error(err.what(), __FILE__, __LINE__, __func__);
			}
		}

		std::pair<Socket, Endpoint> accept() {
			struct sockaddr_in addr;
			socklen_t acc_len = sizeof(addr);
			Socket socket = check("accept()", ::accept(_socket, (struct sockaddr*)&addr, &acc_len));
			socket.linger();
			socket.flags(O_NONBLOCK);
			socket.flags2(FD_CLOEXEC);
			Endpoint endpoint(&addr);
			Logger(Level::COMPONENT) << "Accepted connection from " << endpoint << std::endl;
			return std::make_pair(socket, endpoint);
		}

		void linger() {
//			struct ::linger x;
//			x.l_onoff = 1;
//			x.l_linger = 5;
//			check("linger()", ::setsockopt(_socket, SOL_SOCKET, SO_LINGER, &x, sizeof(x)));
//			options(SO_KEEPALIVE);
		}

		void close() {
			if (_socket > 0) {
				Logger(Level::COMPONENT) << "Closing socket " << _socket << std::endl;
				::shutdown(_socket, SHUT_RDWR);
				check("close()", ::close(_socket));
			}
			_socket = -1;
		}

		void no_reading() {
			if (_socket > 0) {
				check("no_reading()", ::shutdown(_socket, SHUT_RD));
			}
		}

		void flags2(Flag f) { ::fcntl(_socket, F_SETFD, flags2() | f); }
		Flag flags2() const { return ::fcntl(_socket, F_GETFD); }
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
				check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, &ret, &sz));
				// ignore EAGAIN since it is common 'error' in asynchronous programming
				if (ret == EAGAIN) ret = 0;
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

		Endpoint bind_addr() const {
			struct ::sockaddr_in addr;
			socklen_t len = sizeof(addr);
			int ret = ::getsockname(_socket, (struct ::sockaddr*)&addr, &len);
			return ret == -1 ? Endpoint() : Endpoint(&addr);
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
//			return ::write(_socket, buf, size);
			return ::send(_socket, buf, size, MSG_NOSIGNAL);
		}

		operator int() const { return _socket; }
		bool operator==(const Socket& rhs) const { return _socket == rhs._socket; }

		Socket& operator=(const Socket& rhs) {
			this->close();
			_socket = rhs._socket;
			return *this;
		}

		friend std::ostream& operator<<(std::ostream& out, const Socket& rhs) {
			return out << '[' << rhs._socket << ','
				<< (rhs.error() == 0 ? " " : strerror(errno))
				<< ']';
		}

	private:

		static int check_connect(const char* func, int ret) {
			return (errno == EINPROGRESS) ? ret : check(func, ret);
		}

		int _socket;
	};

}
