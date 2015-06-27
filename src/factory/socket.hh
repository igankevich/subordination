namespace factory {

	struct Socket {

		typedef int Flag;
		typedef int Option;

		static const int DEFAULT_FLAGS = SOCK_NONBLOCK | SOCK_CLOEXEC;

		constexpr Socket() noexcept: _socket(0) {}
		constexpr Socket(int socket) noexcept: _socket(socket) {}
		constexpr Socket(const Socket& rhs) noexcept: _socket(rhs._socket) {}
		Socket(Socket&& rhs) noexcept: _socket(rhs._socket) { rhs._socket = INVALID_SOCKET; }

		void create_socket_if_necessary() {
			if (!this->is_valid()) {
				check("socket()", _socket = ::socket(AF_INET, SOCK_STREAM | DEFAULT_FLAGS, 0));
			}
		}

		void bind(Endpoint e) {
			create_socket_if_necessary();
			options(SO_REUSEADDR);
			check("bind()", ::bind(_socket, e.sockaddr(), sizeof(sockaddr_in)));
			Logger<Level::COMPONENT>() << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(_socket, SOMAXCONN));
			Logger<Level::COMPONENT>() << "Listening on " << name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
				create_socket_if_necessary();
				check_connect("connect()", ::connect(_socket, e.sockaddr(), sizeof(sockaddr_in)));
				Logger<Level::COMPONENT>() << "Connecting to " << e << std::endl;
			} catch (std::system_error& err) {
				Logger<Level::COMPONENT>() << "Rethrowing connection error." << std::endl;
				std::stringstream msg;
				msg << err.what() << ". Endpoint=" << e;
				throw Connection_error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}

		std::pair<Socket, Endpoint> accept() {
			Endpoint addr;
			socklen_t len = sizeof(::sockaddr_in);
			Socket socket = check("accept()", ::accept(_socket, addr.sockaddr(), &len));
			socket.flags(O_NONBLOCK);
			socket.flags2(FD_CLOEXEC);
			Logger<Level::COMPONENT>() << "Accepted connection from " << addr << std::endl;
			return std::make_pair(socket, addr);
		}

		void close() {
			if (this->is_valid()) {
				Logger<Level::COMPONENT>() << "Closing socket " << _socket << std::endl;
				::shutdown(_socket, SHUT_RDWR);
				::close(_socket);
//				check("close()", ::close(_socket));
			}
			_socket = INVALID_SOCKET;
		}

		void no_reading() {
			if (this->is_valid()) {
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

		constexpr bool is_valid() const { return _socket > 0; }
		
		int error() const {
			int ret = 0;
			if (!this->is_valid()) {
				ret = -1;
			} else {
				socklen_t sz = sizeof(ret);
				check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, &ret, &sz));
			}
			// ignore EAGAIN since it is common 'error' in asynchronous programming
			if (ret == EAGAIN || ret == EINPROGRESS) {
				ret = 0;
			} else {
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
			}
			return ret;
		}

		Endpoint bind_addr() const {
			Endpoint addr;
			socklen_t len = sizeof(::sockaddr_in);
			int ret = ::getsockname(_socket, addr.sockaddr(), &len);
			return ret == -1 ? Endpoint() : addr;
		}

		Endpoint name() const {
			Endpoint addr;
			socklen_t len = sizeof(::sockaddr_in);
			check("getsockname()", ::getsockname(_socket, addr.sockaddr(), &len));
			return addr;
		}

		Endpoint peer_name() const {
			Endpoint addr;
			socklen_t len = sizeof(::sockaddr_in);
			check("getpeername()", ::getpeername(_socket, addr.sockaddr(), &len));
			return addr;
		}

		uint32_t read(char* buf, size_t size) {
			ssize_t ret = ::read(_socket, buf, size);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		uint32_t write(const char* buf, size_t size) {
			ssize_t ret = ::send(_socket, buf, size, MSG_NOSIGNAL);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		constexpr int fd() const noexcept { return _socket; }
		constexpr operator int() const noexcept { return _socket; }
		constexpr bool operator==(const Socket& rhs) const noexcept { return _socket == rhs._socket; }

		Socket& operator=(const Socket& rhs) {
			this->close();
			_socket = rhs._socket;
			return *this;
		}

		friend std::ostream& operator<<(std::ostream& out, const Socket& rhs) {
			return out << '[' << rhs._socket << ','
				<< (rhs.error() == 0 ? " " : ::strerror(errno))
				<< ']';
		}

	private:

		static int check_connect(const char* func, int ret) {
			return (errno == EINPROGRESS) ? ret : check(func, ret);
		}

	protected:
		int _socket;
		static const int INVALID_SOCKET = -1;
	};

}
namespace factory {

	struct Server_socket: public Socket {
		constexpr Server_socket() {}
		explicit Server_socket(const Socket& rhs): Socket(rhs) {}
		explicit Server_socket(const Endpoint& endp) {
			this->bind(endp);
			this->listen();
		}
		Server_socket(Server_socket&& rhs): Socket(static_cast<Socket&&>(rhs)) {}
		~Server_socket() { this->close(); }

		Server_socket& operator=(const Socket& rhs) {
			Socket::operator=(rhs);
			return *this;
		}
	};

	struct Client_socket: public Socket {
		Client_socket(Endpoint endpoint) { this->connect(endpoint); }
		virtual ~Client_socket() { this->close(); }
	};

}
/*
 * WebSocket lib with support for "wss://" encryption.
 * Copyright 2010 Joel Martin
 * Licensed under LGPL version 3 (see docs/LICENSE.LGPL-3)
 *
 * You can make a cert/key with openssl using:
 * openssl req -new -x509 -days 365 -nodes -out self.pem -keyout self.pem
 * as taken from http://docs.python.org/dev/library/ssl.html#certificates
 */

namespace factory {
	
	struct Web_socket: public Socket {

		struct HTTP_headers {
	
			size_t size() const { return hdrs.size(); }
			std::string& operator[](const std::string& key) { return hdrs[key]; }
	
			bool contain(const char* name) const {
				return hdrs.count(name) != 0;
			}
	
			bool contain(const char* name, const char* value) const {
				auto it = hdrs.find(name);
				return it != hdrs.end() && it->second == value;
			}
	
			bool contain_value(const char* name, const char* value) const {
				auto it = hdrs.find(name);
				return it != hdrs.end() && it->second.find(value) != std::string::npos;
			}
	
		private:
			std::unordered_map<std::string, std::string> hdrs;
		};

		enum struct State: uint8_t {
			PARSING_HTTP_METHOD,
			PARSING_HEADERS,
			PARSING_ERROR,
			PARSING_SUCCESS,
			REPLYING_TO_HANDSHAKE,
			HANDSHAKE_SUCCESS
		};

		Web_socket():
			_http_headers(),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE)
			{}

		explicit Web_socket(const Socket& rhs):
			Socket(rhs),
			_http_headers(),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE)
			{}

		virtual ~Web_socket() {
			this->close();
		}

		Web_socket& operator=(const Socket& rhs) {
			Socket::operator=(rhs);
			return *this;
		}

		uint32_t write(const char* buf, size_t size) {
			uint32_t ret = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				handshake();
			} else {
				websocket_encode(buf, buf + size, std::back_inserter(send_buffer));
				if (this->flush()) {
					ret = size;
				}
			}
			return ret;
		}

		uint32_t read(char* buf, size_t size) {
			uint32_t bytes_read = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				handshake();
			} else {
				Opcode opcode = Opcode::CONT_FRAME;
				if (mid_buffer.empty()) {
					recv_buffer.fill<Socket&>(*this);
					size_t len = websocket_decode(recv_buffer.read_begin(),
						recv_buffer.read_end(), std::back_inserter(mid_buffer),
						&opcode);
					Logger<Level::WEBSOCKET>() << "recv buffer"
						<< "(" << recv_buffer.size() << ") "
						<< recv_buffer << std::endl;
					Logger<Level::WEBSOCKET>() << "mid buffer "
						<< mid_buffer << std::endl;
					recv_buffer.ignore(len);
				}
				if (opcode == Opcode::CONN_CLOSE) {
					Logger<Level::WEBSOCKET>() << "Close frame" << std::endl;
					this->close();
				} else {
					bytes_read = mid_buffer.read(buf, size);
				}
			}
			return bytes_read;
		}

	private:

		template<class It>
		void read_header(It first, It last) {
			Logger<Level::WEBSOCKET>() << "read header line = " << std::string(first, last) << std::endl;
			switch (state) {
				case State::PARSING_HTTP_METHOD: {
					std::string line(first, last);
					if (line.find("GET") == 0) {
						state = State::PARSING_HEADERS;
						Logger<Level::WEBSOCKET>() << "parsing headers" << std::endl;
					} else {
						state = State::PARSING_ERROR;
						Logger<Level::WEBSOCKET>()
							<< "bad method in web socket hand shake" << std::endl;
					}
					} break;
				case State::PARSING_HEADERS: {
					It it = std::find(first, last, ':');
					if (it != last) {
						std::string key(first, it);
						// advance two characters further (colon plus space)
						std::string value(it+2, last);
						if (_http_headers.size() == MAX_HEADERS) {
							state = State::PARSING_ERROR;
							Logger<Level::WEBSOCKET>()
								<< "too many headers in HTTP request" << std::endl;
						} else if (_http_headers.contain(key.c_str())) {
							state = State::PARSING_ERROR;
							Logger<Level::WEBSOCKET>()
								<< "duplicate HTTP header: '" << key << '\'' << std::endl;
						} else {
							_http_headers[key] = value;
							Logger<Level::WEBSOCKET>()
								<< "Header['" << key << "'] = '" << value << "'" << std::endl;
						}
					}
					} break;
				case State::PARSING_ERROR:
				case State::HANDSHAKE_SUCCESS:
				default: break;
			}
		}

		void handshake() {
			State old_state = this->state;
			switch (state) {
				case State::HANDSHAKE_SUCCESS:
					break;
				case State::REPLYING_TO_HANDSHAKE:
					this->flush();
					if (send_buffer.empty()) {
						state = State::HANDSHAKE_SUCCESS;
					}
					break;
				case State::PARSING_SUCCESS:
					reply_success();
					break;
				case State::PARSING_ERROR:
					reply_error();
					break;
				case State::PARSING_HTTP_METHOD:
				case State::PARSING_HEADERS:
				default: {
					read_http_method_and_headers();
					break;
				}
			}
			if (old_state != this->state) {
				handshake();
			}
		}

		bool success() const { return state == State::HANDSHAKE_SUCCESS; }

		bool validate_headers() {
			return
				_http_headers.contain("Sec-WebSocket-Key")
				&& _http_headers.contain("Sec-WebSocket-Version")
				&& _http_headers.contain("Sec-WebSocket-Protocol", "binary")
				&& _http_headers.contain("Upgrade", "websocket")
				&& _http_headers.contain_value("Connection", "Upgrade");
		}

		void read_http_method_and_headers() {
			recv_buffer.fill(Socket(_socket));
			typedef decltype(recv_buffer.begin()) It;
			It pos = recv_buffer.begin();
			It found;
			while ((found = std::search(pos, recv_buffer.end(),
				HTTP_FIELD_SEPARATOR, HTTP_FIELD_SEPARATOR + 2)) != recv_buffer.end()
				&& pos != found
				&& state != State::PARSING_ERROR)
			{
				read_header(pos, found);
				pos = found + 2;
			}
			if (pos == found) {
				recv_buffer.reset();
				if (!validate_headers()) {
					state = State::PARSING_ERROR;
					Logger<Level::WEBSOCKET>() << "parsing error" << std::endl;
				} else {
					state = State::PARSING_SUCCESS;
					Logger<Level::WEBSOCKET>() << "parsing success" << std::endl;
				}
			}
		}

		void reply_success() {
			Logger<Level::WEBSOCKET>() << "replying success" << std::endl;
			std::string accept_header(ACCEPT_HEADER_LENGTH, 0);
			websocket_accept_header(_http_headers["Sec-WebSocket-Key"], accept_header.begin());
			std::stringstream response;
			response
				<< "HTTP/1.1 101 Switching Protocols" << HTTP_FIELD_SEPARATOR
				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Accept: " << accept_header << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
				<< HTTP_FIELD_SEPARATOR;
			std::string buf = response.str();
			send_buffer.write(buf.data(), buf.size());
			state = State::REPLYING_TO_HANDSHAKE;
		}

		void reply_error() {
			send_buffer.write(BAD_REQUEST, sizeof(BAD_REQUEST)-1);
			state = State::REPLYING_TO_HANDSHAKE;
		}

		bool flush() {
			send_buffer.flush(Socket(_socket));
			return send_buffer.empty();
		}

		friend std::ostream& operator<<(std::ostream& out, const Web_socket& rhs) {
			return out << rhs._socket;
		}
	
		State state = State::PARSING_HTTP_METHOD;
		HTTP_headers _http_headers;

		LBuffer<char> send_buffer;
		LBuffer<char> recv_buffer;
		LBuffer<char> mid_buffer;

		static const size_t BUFFER_SIZE = 1024;
		static const size_t MAX_HEADERS = 20;
		static const size_t ACCEPT_HEADER_LENGTH = 29;

		constexpr static const char* HTTP_FIELD_SEPARATOR = "\r\n";
		constexpr static const char* BAD_REQUEST = "HTTP/2.1 400 Bad Request\r\n\r\n";
	};

}
