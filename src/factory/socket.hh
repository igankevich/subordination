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
			check("bind()", ::bind(_socket, e.sockaddr(), e.sockaddrlen()));
			Logger<Level::COMPONENT>() << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(_socket, SOMAXCONN));
			Logger<Level::COMPONENT>() << "Listening on " << name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
				create_socket_if_necessary();
				check_connect("connect()", ::connect(_socket, e.sockaddr(), e.sockaddrlen()));
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
			Endpoint::Sock_len len = sizeof(Endpoint);
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
				Endpoint::Sock_len sz = sizeof(ret);
				check("getsockopt()", ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, &ret, &sz));
			}
			// ignore EAGAIN since it is common 'error' in asynchronous programming
			if (ret == EAGAIN || ret == EINPROGRESS) {
				ret = 0;
			} else {
				// If one connects to localhost to a different port and the service is offline
				// then socket's local port can be chosen to be the same as the port of the service.
				// If this happens the socket connects to itself and sends and replies to
				// its own messages (at least on Linux). This conditional solves the issue.
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
			Endpoint::Sock_len len = sizeof(Endpoint);
			int ret = ::getsockname(_socket, addr.sockaddr(), &len);
			return ret == -1 ? Endpoint() : addr;
		}

		Endpoint name() const {
			Endpoint addr;
			Endpoint::Sock_len len = sizeof(Endpoint);
			check("getsockname()", ::getsockname(_socket, addr.sockaddr(), &len));
			return addr;
		}

		Endpoint peer_name() const {
			Endpoint addr;
			Endpoint::Sock_len len = sizeof(Endpoint);
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

		// TODO: remove this ``boilerplate''
		constexpr bool empty() const { return true; }
		constexpr bool flush() const { return true; }

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

			bool contain(const char* name, const std::string& value) const {
				auto it = hdrs.find(name);
				return it != hdrs.end() && it->second == value;
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
			INITIAL_STATE,
			PARSING_HTTP_METHOD,
			PARSING_HTTP_STATUS,
			PARSING_HEADERS,
			PARSING_ERROR,
			PARSING_SUCCESS,
			REPLYING_TO_HANDSHAKE,
			HANDSHAKE_SUCCESS,
			STARTING_HANDSHAKE,
			WRITING_HANDSHAKE
		};

		enum struct Role: uint8_t { SERVER, CLIENT };

		Web_socket():
			_http_headers(),
			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE)
			{}

		explicit Web_socket(const Socket& rhs):
			Socket(rhs),
			_http_headers(),
			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE)
			{}

		virtual ~Web_socket() { this->close(); }

		Web_socket& operator=(const Socket& rhs) {
			Socket::operator=(rhs);
			return *this;
		}

		uint32_t write(const char* buf, size_t size) {
			uint32_t ret = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				if (state == State::INITIAL_STATE) {
					state = State::STARTING_HANDSHAKE;
					role = Role::CLIENT;
				}
				handshake();
			} else {
				websocket_encode(buf, buf + size, std::back_inserter(send_buffer));
				this->flush();
				ret = size;
			}
			return ret;
		}

		uint32_t read(char* buf, size_t size) {
			uint32_t bytes_read = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				if (state == State::INITIAL_STATE) {
					state = State::PARSING_HTTP_METHOD;
					role = Role::SERVER;
				}
				handshake();
			} else {
				Opcode opcode = Opcode::CONT_FRAME;
				if (mid_buffer.empty()) {
					this->fill();
					size_t len = websocket_decode(recv_buffer.read_begin(),
						recv_buffer.read_end(), std::back_inserter(mid_buffer),
						&opcode);
					Logger<Level::WEBSOCKET>() << "recv buffer"
						<< "(" << recv_buffer.size() << ") "
						<< std::setw(40) << recv_buffer << std::endl;
//					Logger<Level::WEBSOCKET>() << "mid buffer "
//						<< std::setw(40) << mid_buffer << std::endl;
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

		bool empty() const { return send_buffer.empty(); }

		bool flush() {
			size_t old_size = send_buffer.size();
			send_buffer.flush(Socket(_socket));
			Logger<Level::WEBSOCKET>() << "send buffer"
				<< "(" << old_size - send_buffer.size() << ") "
				<< std::endl;
			return send_buffer.empty();
		}

	private:

		template<class It>
		void read_header(It first, It last) {
			switch (state) {
				case State::PARSING_HTTP_STATUS: parse_http_status(std::string(first, last)); break;
				case State::PARSING_HTTP_METHOD: parse_http_method(first, last); break;
				case State::PARSING_HEADERS: parse_http_headers(first, last); break;
				case State::PARSING_ERROR:
				case State::PARSING_SUCCESS:
				case State::HANDSHAKE_SUCCESS:
				case State::INITIAL_STATE:
				case State::REPLYING_TO_HANDSHAKE:
				case State::STARTING_HANDSHAKE:
				case State::WRITING_HANDSHAKE:
				default: break;
			}
		}

		void parse_http_status(std::string line) {
			if (line.find("HTTP") == 0) {
				auto sep1 = line.find(' ');
				if (sep1 != std::string::npos) {
					auto sep2 = line.find(' ', sep1);
					if (sep2 != std::string::npos && line.compare(sep1, sep2-sep1, "101")) {
						state = State::PARSING_HEADERS;
						Logger<Level::WEBSOCKET>() << "parsing headers" << std::endl;
					} else {
						state = State::PARSING_ERROR;
						Logger<Level::WEBSOCKET>()
							<< "bad HTTP status in web socket hand shake" << std::endl;
					}
				}
			} else {
				state = State::PARSING_ERROR;
				Logger<Level::WEBSOCKET>()
					<< "bad method in web socket hand shake" << std::endl;
			}
		}

		template<class It>
		void parse_http_method(It first, It last) {
			static const char GET[] = "GET";
			if (std::search(first, last, GET, GET + sizeof(GET)-1) != last) {
				state = State::PARSING_HEADERS;
				Logger<Level::WEBSOCKET>() << "parsing headers" << std::endl;
			} else {
				state = State::PARSING_ERROR;
				Logger<Level::WEBSOCKET>()
					<< "bad method in web socket hand shake" << std::endl;
			}
		}

		template<class It>
		void parse_http_headers(It first, It last) {
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
		}

		bool is_server() const { return role == Role::SERVER; }
		bool is_client() const { return role == Role::CLIENT; }

		void handshake() {
			State old_state = this->state;
			switch (state) {
				case State::HANDSHAKE_SUCCESS: break;
				case State::REPLYING_TO_HANDSHAKE:
					this->flush();
					if (send_buffer.empty()) {
						state = State::HANDSHAKE_SUCCESS;
					}
					break;
				case State::PARSING_SUCCESS:
					if (is_server()) reply_success(); else state = State::HANDSHAKE_SUCCESS;
					break;
				case State::PARSING_ERROR:
					is_server() ? reply_error() : this->close();
					break;
				case State::STARTING_HANDSHAKE:
					start_handshake();
					break;
				case State::WRITING_HANDSHAKE:
					if (this->flush()) {
						state = State::PARSING_HTTP_STATUS;
					}
					break;
				case State::INITIAL_STATE:
				case State::PARSING_HTTP_METHOD:
				case State::PARSING_HTTP_STATUS:
				case State::PARSING_HEADERS:
					read_http_method_and_headers();
					break;
				default: break;
			}
			if (old_state != this->state) {
				handshake();
			}
		}

		bool validate_headers() const {
			bool ret;
			if (is_client()) {
				std::string valid_accept(ACCEPT_HEADER_LENGTH, 0);
				websocket_accept_header(this->key, valid_accept.begin());
				ret = _http_headers.contain("Sec-WebSocket-Accept", valid_accept);
			} else {
				ret = _http_headers.contain("Sec-WebSocket-Key")
					&& _http_headers.contain("Sec-WebSocket-Version");
			}
			ret = ret 
				&& _http_headers.contain("Sec-WebSocket-Protocol", "binary")
				&& _http_headers.contain("Upgrade", "websocket")
				&& _http_headers.contain_value("Connection", "Upgrade");
			return ret;
		}

		void read_http_method_and_headers() {
			this->fill();
			typedef decltype(recv_buffer.begin()) It;
			It pos = recv_buffer.begin();
			It found;
			while ((found = std::search(pos, recv_buffer.end(),
				HTTP_FIELD_SEPARATOR, HTTP_FIELD_SEPARATOR + 2)) != recv_buffer.end()
				&& pos != found && state != State::PARSING_ERROR)
			{
				read_header(pos, found);
				pos = found + 2;
			}
			if (pos == found && pos != recv_buffer.begin()) {
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

		void start_handshake() {
			websocket_key(this->key.begin());
			std::stringstream request;
			request
				<< "GET / HTTP/1.1" << HTTP_FIELD_SEPARATOR
				<< "User-Agent: Factory/" << VERSION << HTTP_FIELD_SEPARATOR
				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Version: 13" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Key: " << key << HTTP_FIELD_SEPARATOR
				<< HTTP_FIELD_SEPARATOR;
			std::string buf = request.str();
			send_buffer.write(buf.data(), buf.size());
			state = State::WRITING_HANDSHAKE;
			Logger<Level::WEBSOCKET>() << "writing handshake " << request.str() << std::endl;
		}

		void fill() { recv_buffer.fill(Socket(_socket)); }

		State state = State::INITIAL_STATE;
		Role role = Role::SERVER;
		HTTP_headers _http_headers;
		std::string key;

		LBuffer<char> send_buffer;
		LBuffer<char> recv_buffer;
		LBuffer<char> mid_buffer;

		static const size_t BUFFER_SIZE = 1024*16;
		static const size_t MAX_HEADERS = 20;
		static const size_t ACCEPT_HEADER_LENGTH = 29;
		static const size_t WEBSOCKET_KEY_BASE64_LENGTH = 24;

		constexpr static const char* HTTP_FIELD_SEPARATOR = "\r\n";
		constexpr static const char* BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\n\r\n";
	};

	template<class T>
	struct basic_fdbuf: public std::basic_streambuf<T> {

		using typename std::basic_streambuf<T>::int_type;
		using typename std::basic_streambuf<T>::traits_type;
		using typename std::basic_streambuf<T>::char_type;
		typedef std::ios_base::openmode openmode;
		typedef std::ios_base::seekdir seekdir;
		typedef int fd_type;

		explicit basic_fdbuf(fd_type fd, std::size_t gbufsize, std::size_t pbufsize, std::size_t nputback=1):
			_fd(fd),
			_nputback(nputback),
			_gbuf(std::max(gbufsize, nputback) + nputback),
			_pbuf(pbufsize)
		{
			char_type* end = &_gbuf.front() + _gbuf.size();
			this->setg(end, end, end);
			char_type* beg = &this->_pbuf.front();
			this->setp(beg, beg + this->_pbuf.size());
		}

		basic_fdbuf(basic_fdbuf&& rhs):
			_fd(rhs._fd),
			_nputback(rhs._nputback),
			_gbuf(std::move(rhs._gbuf)),
			_pbuf(std::move(rhs._pbuf))
			{}

		virtual ~basic_fdbuf() {
			// TODO: for now full sync is not guaranteed for non-blocking I/O
			// as it may result in infinite loop
			this->sync();
		}

		int_type underflow() {
			if (this->gptr() != this->egptr()) {
				return traits_type::to_int_type(*this->gptr());
			}
			char_type* base = &_gbuf.front();
			char_type* start = this->initputbackbuf(base);
			ssize_t n = ::read(_fd, start, _gbuf.size() - (start - base));
			std::clog << "Reading fd=" << _fd
				<< ",n=" << n << std::endl;
			if (n <= 0) {
				return traits_type::eof();
			}
			this->setg(base, start, start + n);
			return traits_type::to_int_type(*this->gptr());
		}

		int_type overflow(int_type c = traits_type::eof()) {
			if (c != traits_type::eof()) {
				if (this->pptr() == this->epptr()) {
					if (this->sync() == -1) {
						return traits_type::eof();
					}
				}
				if (this->pptr() != this->epptr()) {
					*this->pptr() = c;
					this->pbump(1);
					return traits_type::to_int_type(c);
				}
			} else {
				this->sync();
			}
			return traits_type::eof();
		}

		int_type sync() {
			if (this->pptr() == this->pbase()) return -1;
			ssize_t n = ::write(this->_fd, this->pbase(), this->pptr() - this->pbase());
			if (n <= 0) {
				return -1;
			}
			this->pbump(-n);
			std::clog << "Writing fd=" << this->_fd << ",n=" << n << std::endl;
			return n >= 0 ? 0 : -1;
		}

		std::streampos seekoff(std::streamoff off, seekdir way,
			openmode which = std::ios_base::in | std::ios_base::out)
		{
			if (way & std::ios_base::beg) {
				return this->seekpos(off, which);
			}
			if (way & std::ios_base::cur) {
				std::streampos pos = which & std::ios_base::in
					? static_cast<std::streampos>(this->gptr() - this->eback())
					: static_cast<std::streampos>(this->pptr() - this->pbase());
				return off == 0 ? pos 
					: this->seekpos(pos + off, which);
			}
			if (way & std::ios_base::end) {
				std::streampos pos = which & std::ios_base::in
					? static_cast<std::streampos>(this->egptr() - this->eback())
					: static_cast<std::streampos>(this->epptr() - this->pbase());
				return this->seekpos(pos + off, which);
			}
			return std::streampos(std::streamoff(-1));
		}

		std::streampos seekpos(std::streampos pos, openmode mode = std::ios_base::in | std::ios_base::out) {
			if (mode & std::ios_base::in) {
				std::size_t size = this->_gbuf.size();
				char_type* old_end = this->egptr();
				// go back if the buffer allows it
				if (pos >= 0 && pos < size) {
					char_type* beg = this->eback();
					char_type* end = this->egptr();
					char_type* xgptr = beg+pos;
					if (xgptr < end) {
						this->setg(beg, xgptr, end);
					} else {
						ssize_t n = ::read(this->_fd, end, xgptr-end+1);
						this->setg(beg, end+n, end+n+1);
					}
				}
				// enlarge buffer
				if (pos >= size) {
					std::size_t new_size = calc_size(pos);
					if (new_size != size) {
						std::ptrdiff_t off = this->gptr() - this->eback();
						std::ptrdiff_t n = this->egptr() - this->gptr();
						this->_gbuf.resize(new_size);
						std::clog << "Resize gbuf size="
							<< this->_gbuf.size() << std::endl;
						char_type* base = &this->_gbuf.front();
						this->setg(base, base + off, base + n);
						// fill the buffer
						ssize_t m = ::read(_fd, base + n,
							static_cast<std::ptrdiff_t>(pos) - n + 1);
						std::clog << "Reading fd=" << _fd
							<< ",n=" << m << std::endl;
						if (m > 0) {
							this->setg(base, base + off, base + n + m);
						}
					}
				}
				// always return current position
				return static_cast<std::streampos>(this->gptr() - this->eback());
			}
			return std::streampos(std::streamoff(-1));
		}

		basic_fdbuf& operator=(basic_fdbuf&) = delete;
		basic_fdbuf(basic_fdbuf&) = delete;
	
	private:
		std::size_t calc_size(std::streampos pos) {
			std::size_t base_size = this->_gbuf.size();
			std::size_t target_size = pos;
			while (base_size < target_size
				&& base_size <= std::numeric_limits<std::size_t>::max()/2)
			{
				base_size *= 2;
			}
			return base_size;
		}

	protected:

		char_type* initputbackbuf(char_type* base) {
			char_type* start = base;
			if (this->eback() == base && this->_nputback != 0) {
				traits_type::move(base, this->egptr() - this->_nputback, this->_nputback);
				start += this->_nputback;
			}
			return start;
		}

		fd_type _fd;
		std::size_t _nputback;
		std::vector<char_type> _gbuf;
		std::vector<char_type> _pbuf;
	};

	template<class T>
	struct basic_websocketbuf: public basic_fdbuf<T> {

		using typename std::basic_streambuf<T>::int_type;
		using typename std::basic_streambuf<T>::traits_type;
		using typename std::basic_streambuf<T>::char_type;
		using typename basic_fdbuf<T>::fd_type;

		enum struct State {
			INITIAL_STATE,
			PARSING_HTTP_METHOD,
			PARSING_HTTP_STATUS,
			PARSING_HEADERS,
			PARSING_ERROR,
			PARSING_SUCCESS,
			REPLYING_TO_HANDSHAKE,
			HANDSHAKE_SUCCESS,
			STARTING_HANDSHAKE,
			WRITING_HANDSHAKE,
			READING_FRAME,
			READING_PAYLOAD
		};

		enum struct Role { SERVER, CLIENT };
		
		explicit basic_websocketbuf(fd_type fd, std::size_t bufsize=512, std::size_t nputback=1):
			basic_fdbuf<T>(fd, bufsize, bufsize, nputback) {}

		basic_websocketbuf(basic_websocketbuf&& rhs):
			basic_fdbuf<T>(std::move(rhs)),
			_state(rhs._state),
			_role(rhs._role),
			_frame(rhs._frame),
			_nread(rhs._nread)
			{}

		int_type underflow() {
			if (this->gptr() != this->egptr()) {
				return traits_type::to_int_type(*this->gptr());
			}
			char_type* base = &this->_gbuf.front();
			char_type* start = this->initputbackbuf(base);
			// initialise ``server'' role
			initserver();
			// perform handshake if any
//			handshake();
			if (this->state() != State::HANDSHAKE_SUCCESS) {
				return traits_type::eof();
			}
			// fill buffer from fd
			ssize_t n = ::read(this->_fd, start, this->_gbuf.size() - (start - base));
			std::clog << "Reading fd=" << this->_fd << ",n=" << n << std::endl;
			if (n <= 0) {
				return traits_type::eof();
			}
			this->setg(base, start, start + n);
			// read frame header
			if (this->state() == State::READING_FRAME) {
				size_t hdrsz = _frame.decode_header(this->gptr(), this->egptr());
				if (hdrsz > 0) {
					this->_nread = 0;
					this->gbump(hdrsz);
					this->sets(State::READING_PAYLOAD);
					if (_frame.opcode() == Opcode::CONN_CLOSE) {
						Logger<Level::WEBSOCKET>() << "Close frame" << std::endl;
						check("close()", ::close(this->_fd));
					}
					// TODO: validate frame
				}
			}
			// read payload
			if (this->state() == State::READING_PAYLOAD) {
				if (this->gptr() == this->egptr()) {
					return traits_type::eof();
				}
				if (this->_nread == _frame.payload_size()) {
					this->sets(State::READING_FRAME);
					return this->underflow();
				}
				char_type ch = _frame.getpayloadc(this->gptr(), this->_nread);
				this->gbump(1);
				++this->_nread;
				return traits_type::to_int_type(ch);
			}
			return traits_type::eof();
		}

		basic_websocketbuf& operator=(basic_websocketbuf&) = delete;
		basic_websocketbuf(basic_websocketbuf&) = delete;

	private:

		void sets(State rhs) { _state = rhs; }
		State state() const { return _state; }
		void setrole(Role rhs) { _role = rhs; }
		State role() const { return _role; }

		void initserver() {
			if (this->state() == State::INITIAL_STATE) {
				this->sets(State::PARSING_HTTP_METHOD);
				this->setrole(Role::SERVER);
			}
		}

		State _state = State::INITIAL_STATE;
		Role _role = Role::SERVER;
		Web_socket_frame _frame = {};
		std::size_t _nread = 0;
	};

	template<class T>
	struct basic_fd_istream: public std::istream {
		typedef typename basic_fdbuf<T>::fd_type fd_type;
		explicit basic_fd_istream(fd_type fd): std::istream(new basic_fdbuf<T>(fd, 512, 0)) {}
		virtual ~basic_fd_istream() { delete this->rdbuf(); }
	};

	template<class T>
	struct basic_fd_ostream: public std::ostream {
		typedef typename basic_fdbuf<T>::fd_type fd_type;
		explicit basic_fd_ostream(fd_type fd): std::ostream(new basic_fdbuf<T>(fd, 0, 512)) {}
		virtual ~basic_fd_ostream() { delete this->rdbuf(); }
	};

	template<class T>
	struct basic_fd_iostream: public std::iostream {
		typedef typename basic_fdbuf<T>::fd_type fd_type;
		explicit basic_fd_iostream(fd_type fd): std::iostream(new basic_fdbuf<T>(fd, 512, 512)) {}
		virtual ~basic_fd_iostream() { delete this->rdbuf(); }
	};

	typedef basic_fdbuf<char> fdbuf;
	typedef basic_fd_istream<char> fd_istream;
	typedef basic_fd_ostream<char> fd_ostream;

}
