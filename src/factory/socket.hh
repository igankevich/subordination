namespace factory {

	enum Socket_type {
		RELIABLE_SOCKET = SOCK_STREAM,
		UNRELIABLE_SOCKET = SOCK_DGRAM
	};

	struct Socket {

		typedef int Flag;
		typedef int Option;

		static const int DEFAULT_FLAGS = SOCK_NONBLOCK | SOCK_CLOEXEC;

		Socket(): _socket(0) {}
		Socket(int socket): _socket(socket) {}
		Socket(const Socket& rhs): _socket(rhs._socket) {}

		void listen(Endpoint e, Socket_type type = RELIABLE_SOCKET) {
//			struct sockaddr_in addr;
//			e.to_sockaddr(&addr);
//			init_socket_address(&addr, e.host().c_str(), e.port());
			check("socket()", _socket = ::socket(AF_INET, type | DEFAULT_FLAGS, 0));
			options(SO_REUSEADDR);
			check("bind()", ::bind(_socket, (struct sockaddr*)e.addr(), sizeof(sockaddr_in)));
			factory_log(Level::COMPONENT) << "Binding to " << e << std::endl;
			if (type == RELIABLE_SOCKET) {
				check("listen()", ::listen(_socket, SOMAXCONN));
				factory_log(Level::COMPONENT) << "Listening on " << e << std::endl;
			}
		}

		void connect(Endpoint e, Socket_type type = RELIABLE_SOCKET) {
			try {
//				struct sockaddr_in addr;
//				init_socket_address(&addr, e.host().c_str(), e.port());
//				e.to_sockaddr(&addr);
				check("socket()", _socket = ::socket(AF_INET, type | DEFAULT_FLAGS, 0));
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
			if (_socket > 0) {
				factory_log(Level::COMPONENT) << "Closing socket " << _socket << std::endl;
				::shutdown(_socket, SHUT_RDWR);
				::close(_socket);
			}
			_socket = 0;
		}

		void flags(Flag f) { ::fcntl(_socket, F_SETFL, flags() | f); }
		Flag flags() const { return ::fcntl(_socket, F_GETFL); }

		void options(Option option) {
			int one = 1;
			check("setsockopt()", ::setsockopt(_socket, SOL_SOCKET, option, &one, sizeof(one)));
		}

		int error() const {
			int ret = 0;
			if (_socket <= 0) {
				ret = -1;
			} else {
				socklen_t sz = sizeof(ret);
				factory_log(Level::COMPONENT) << "Socket = " << _socket << std::endl;
				check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, &ret, &sz));
				factory_log(Level::COMPONENT) << "getsockopt(): " << ::strerror(ret) << std::endl;
			}
			return ret;
		}

//		Socket_type type() const {
//			int tp = SOCK_STREAM;
//			socklen_t tp_size = sizeof(tp);
//			check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_TYPE, &tp, &tp_size));
//			return Socket_type(tp);
//		}

		ssize_t read(char* buf, size_t size) {
			return ::read(_socket, buf, size);
		}

		ssize_t write(const char* buf, size_t size) {
			return ::write(_socket, buf, size);
		}

		Endpoint from() const {
			char dummy;
			struct ::sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);
			check("recvfrom()", ::recvfrom(_socket, &dummy, 0, MSG_PEEK, (struct ::sockaddr*)&addr, &addr_len));
			return Endpoint(&addr);
		}

//		void send(Foreign_stream& packet) {
//			packet.insert_size();
//			std::stringstream msg;
//			msg << "write(" << _socket<< ')';
//			check(msg.str().c_str(), ::write(_socket, packet.buffer(), packet.size()));
//		}

		operator int() const { return _socket; }
		bool operator==(const Socket& rhs) const { return _socket == rhs._socket; }
		Socket& operator=(const Socket& rhs) { _socket = rhs._socket; return *this; }

		friend std::ostream& operator<<(std::ostream& out, const Socket& rhs) {
			return out << rhs._socket;
		}

		bool is_valid() const { return _socket > 0; }

	private:

		// TODO: connection status (success or failure) should be checked asynchronously in Socket_server
		static int check_connect(const char* func, int ret) {
			return (errno == EINPROGRESS) ? ret : check(func, ret);
		}

//		static void init_socket_address(struct sockaddr_in* addr, const char* hostname, Port port) {
//			std::memset(addr, 0, sizeof(sockaddr_in));
//			addr->sin_family = AF_INET;
//			addr->sin_port = htons(port);
//			if (check("inet_pton()", ::inet_pton(AF_INET, hostname, &addr->sin_addr.s_addr)) == 0) {
//				inet_address(hostname, addr);
//			}
//		}

		int _socket;
	};

}
