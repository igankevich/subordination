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
				check("socket()", this->_socket = ::socket(AF_INET, SOCK_STREAM | DEFAULT_FLAGS, 0));
#if !HAVE_DECL_SOCK_NONBLOCK
				this->flags(O_NONBLOCK|O_CLOEXEC);
#endif
			}
		}

		void bind(Endpoint e) {
			this->create_socket_if_necessary();
			this->options(SO_REUSEADDR);
			check("bind()", ::bind(this->_socket, e.sockaddr(), e.sockaddrlen()));
			Logger<Level::COMPONENT>() << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(this->_socket, SOMAXCONN));
			Logger<Level::COMPONENT>() << "Listening on " << this->name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
				this->create_socket_if_necessary();
				this->check_connect("connect()", ::connect(this->_socket, e.sockaddr(), e.sockaddrlen()));
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
			Socket socket = check("accept()", ::accept(this->_socket, addr.sockaddr(), &len));
			socket.flags(O_NONBLOCK);
			socket.flags2(FD_CLOEXEC);
			Logger<Level::COMPONENT>() << "Accepted connection from " << addr << std::endl;
			return std::make_pair(socket, addr);
		}

		void close() {
			if (this->is_valid()) {
				Logger<Level::COMPONENT>() << "Closing socket " << this->_socket << std::endl;
				::shutdown(this->_socket, SHUT_RDWR);
				::close(this->_socket);
//				check("close()", ::close(this->_socket));
			}
			_socket = INVALID_SOCKET;
		}

		void no_reading() {
			if (this->is_valid()) {
				check("no_reading()", ::shutdown(this->_socket, SHUT_RD));
			}
		}

		void flags2(Flag f) { ::fcntl(this->_socket, F_SETFD, this->flags2() | f); }
		Flag flags2() const { return ::fcntl(this->_socket, F_GETFD); }
		void flags(Flag f) { ::fcntl(this->_socket, F_SETFL, this->flags() | f); }
		Flag flags() const { return ::fcntl(this->_socket, F_GETFL); }

		void options(Option option) {
			int one = 1;
			check("setsockopt()", ::setsockopt(this->_socket, SOL_SOCKET, option, &one, sizeof(one)));
		}

		int error() const {
			int ret = 0;
			int opt = 0;
			if (!this->is_valid()) {
				ret = -1;
			} else {
				Endpoint::Sock_len sz = sizeof(ret);
				int e = ::getsockopt(this->_socket, SOL_SOCKET, SO_ERROR, &opt, &sz);
				if (errno == ENOTSOCK) {
					ret = 0;
				} else {
					check("getsockopt()", e);
				}
			}
			// ignore EAGAIN since it is common 'error' in asynchronous programming
			if (opt == EAGAIN || opt == EINPROGRESS) {
				ret = 0;
			} else {
				// If one connects to localhost to a different port and the service is offline
				// then socket's local port can be chosen to be the same as the port of the service.
				// If this happens the socket connects to itself and sends and replies to
				// its own messages (at least on Linux). This conditional solves the issue.
				try {
					if (ret == 0 && this->name() == this->peer_name()) {
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
			int ret = ::getsockname(this->_socket, addr.sockaddr(), &len);
			return ret == -1 ? Endpoint() : addr;
		}

		Endpoint name() const {
			Endpoint addr;
			Endpoint::Sock_len len = sizeof(Endpoint);
			check("getsockname()", ::getsockname(this->_socket, addr.sockaddr(), &len));
			return addr;
		}

		Endpoint peer_name() const {
			Endpoint addr;
			Endpoint::Sock_len len = sizeof(Endpoint);
			check("getpeername()", ::getpeername(this->_socket, addr.sockaddr(), &len));
			return addr;
		}

		uint32_t read(void* buf, size_t size) {
			ssize_t ret = ::read(this->_socket, buf, size);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		uint32_t write(const void* buf, size_t size) {
			ssize_t ret = ::write(this->_socket, buf, size);
//			ssize_t ret = ::send(_socket, buf, size, MSG_NOSIGNAL);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		constexpr int fd() const noexcept { return this->_socket; }
		constexpr operator int() const noexcept { return this->_socket; }
		constexpr bool operator==(const Socket& rhs) const noexcept { return this->_socket == rhs._socket; }
		constexpr bool is_valid() const { return this->_socket > 0; }

		Socket& operator=(const Socket& rhs) {
			this->close();
			this->_socket = rhs._socket;
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
	
	struct Web_socket: public Socket {

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
			WRITING_HANDSHAKE
		};

		enum struct Role { SERVER, CLIENT };

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

	template<class Fd>
	struct fd_wrapper {
		fd_wrapper(Fd f): _fd(f) {}
		ssize_t read(void* buf, std::size_t n) {
			return this->_fd.read(buf, n);
		}
		ssize_t write(const void* buf, std::size_t n) {
			return this->_fd.write(buf, n);
		}
		operator Fd() const { return this->_fd; }
	private:
		Fd _fd;
	};

	template<>
	struct fd_wrapper<int> {
		fd_wrapper(int f): _fd(f) {}
		ssize_t read(void* buf, std::size_t n) {
			return ::read(this->_fd, buf, n);
		}
		ssize_t write(const void* buf, std::size_t n) {
			return ::write(this->_fd, buf, n);
		}
		operator int() const { return this->_fd; }
	private:
		int _fd;
	};

	template<class T, class Fd=int>
	struct basic_fdbuf: public std::basic_streambuf<T> {

		using typename std::basic_streambuf<T>::int_type;
		using typename std::basic_streambuf<T>::traits_type;
		using typename std::basic_streambuf<T>::char_type;
		using typename std::basic_streambuf<T>::pos_type;
		using typename std::basic_streambuf<T>::off_type;
		typedef std::ios_base::openmode openmode;
		typedef std::ios_base::seekdir seekdir;
		typedef Fd fd_type;

		basic_fdbuf(): basic_fdbuf(-1, 512, 512) {}

		basic_fdbuf(fd_type fd, std::size_t gbufsize, std::size_t pbufsize):
			_fd(fd),
			_gbuf(gbufsize),
			_pbuf(pbufsize)
		{
			char_type* end = &_gbuf.front() + _gbuf.size();
			this->setg(end, end, end);
			char_type* beg = &this->_pbuf.front();
			this->setp(beg, beg + this->_pbuf.size());
		}

		basic_fdbuf(basic_fdbuf&& rhs):
			_fd(rhs._fd),
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
			if (this->eback() == this->gptr()) {
				char_type* base = &this->_gbuf.front();
				ssize_t n = this->_fd.read(base, this->_gbuf.size());
				std::clog << "Reading fd=" << _fd << ",n=" << n << std::endl;
				if (n <= 0) {
					return traits_type::eof();
				}
				this->setg(base, base, base + n);
			} else {
				this->growgbuf(this->_gbuf.size() * 2);
				ssize_t n = 0;
				pos_type old_pos = this->gptr() - this->eback();
				pos_type pos = this->egptr() - this->eback();
				while ((n = this->_fd.read(this->eback() + pos, this->_gbuf.size() - pos)) > 0) {
					pos += n;
					if (pos == this->_gbuf.size()) {
						this->growgbuf(this->_gbuf.size() * 2);
					}
				}
				if (pos == old_pos) {
					return traits_type::eof();
				}
				char_type* base = this->eback();
				this->setg(base, base + old_pos, base + pos);
			}
			return traits_type::not_eof(*this->gptr());
		}

		int_type overflow(int_type c = traits_type::eof()) {
			if (c != traits_type::eof()) {
				if (this->pptr() == this->epptr()) {
					this->growpbuf(this->_pbuf.size() * 2);
				// TODO: do we need sync???
//					if (this->sync() == -1) {
//					}
				}
				if (this->pptr() != this->epptr()) {
					*this->pptr() = c;
					this->pbump(1);
					return traits_type::not_eof(c);
				}
			} else {
				// TODO: do we need sync???
//				this->sync();
			}
			return traits_type::eof();
		}

		int sync() {
			std::clog << "Sync" << std::endl;
			if (this->pptr() == this->pbase()) return 0;
			ssize_t n = this->_fd.write(this->pbase(), this->pptr() - this->pbase());
			if (n <= 0) {
				return -1;
			}
			this->pbump(-n);
			std::clog << "Writing fd=" << this->_fd << ",n=" << n << std::endl;
			return n >= 0 ? 0 : -1;
		}

		pos_type seekoff(off_type off, seekdir way,
			openmode which = std::ios_base::in | std::ios_base::out)
		{
			if (way == std::ios_base::beg) {
				std::clog << "seekoff way=beg,off=" << off << std::endl;
				return this->seekpos(off, which);
			}
			if (way == std::ios_base::cur) {
				std::clog << "seekoff way=cur,off=" << off << std::endl;
				pos_type pos = which & std::ios_base::in
					? static_cast<pos_type>(this->gptr() - this->eback())
					: static_cast<pos_type>(this->pptr() - this->pbase());
				return off == 0 ? pos 
					: this->seekpos(pos + off, which);
			}
			if (way == std::ios_base::end) {
				std::clog << "seekoff way=end,off=" << off << std::endl;
				pos_type pos = which & std::ios_base::in
					? static_cast<pos_type>(this->egptr() - this->eback())
					: static_cast<pos_type>(this->epptr() - this->pbase());
				return this->seekpos(pos + off, which);
			}
			return pos_type(off_type(-1));
		}

		pos_type seekpos(pos_type pos,
			openmode mode = std::ios_base::in | std::ios_base::out)
		{
			std::clog << "seekpos " << pos << std::endl;
			if (mode & std::ios_base::in) {
				std::size_t size = this->egptr() - this->eback();
				if (pos >= 0 && pos <= size) {
					char_type* beg = this->eback();
					char_type* end = this->egptr();
					char_type* xgptr = beg+pos;
					this->setg(beg, xgptr, end);
				}
				// always return current position
				return static_cast<pos_type>(this->gptr() - this->eback());
			}
			if (mode & std::ios_base::out) {
				std::size_t size = this->_pbuf.size();
				if (pos >= 0 && pos <= size) {
					std::ptrdiff_t off = this->pptr() - this->pbase();
					this->pbump(static_cast<std::ptrdiff_t>(pos)-off);
				}
//				// enlarge buffer
//				if (pos > size) {
//					std::clog << "GROW: pos=" << pos << std::endl;
//					this->growpbuf(pos);
//					this->pbump(this->epptr() - this->pptr());
//				}
				// always return current position
				return static_cast<pos_type>(this->pptr() - this->pbase());
			}
			return pos_type(off_type(-1));
		}

		void setfd(fd_type rhs) { this->_fd = rhs; }

		basic_fdbuf& operator=(basic_fdbuf&) = delete;
		basic_fdbuf(basic_fdbuf&) = delete;
	
	private:
		static
		std::size_t calc_size(std::size_t target_size, std::size_t base_size) {
			while (base_size < target_size
				&& base_size <= std::numeric_limits<std::size_t>::max()/2)
			{
				base_size *= 2;
			}
			return base_size;
		}

	protected:

		void growgbuf(std::size_t target_size) {
			if (target_size <= this->_gbuf.size()) return;
			std::size_t size = this->_gbuf.size();
			std::size_t new_size = calc_size(target_size, size);
			std::ptrdiff_t off = this->gptr() - this->eback();
			std::ptrdiff_t n = this->egptr() - this->eback();
			this->_gbuf.resize(new_size);
			std::clog << "Resize gbuf size="
				<< this->_gbuf.size() << std::endl;
			char_type* base = &this->_gbuf.front();
			this->setg(base, base + off, base + n);
		}

		void growpbuf(std::size_t target_size) {
			if (target_size <= this->_pbuf.size()) return;
			std::size_t size = this->_pbuf.size();
			std::size_t new_size = calc_size(target_size, size);
			std::ptrdiff_t off = this->pptr() - this->pbase();
			std::ptrdiff_t n = this->epptr() - this->pbase();
			this->_pbuf.resize(new_size);
			std::clog << "Resize pbuf size=" << new_size << std::endl;
			char_type* base = &this->_pbuf.front();
			this->setp(base, base + new_size);
			this->pbump(off);
		}

		std::size_t pbufsize() const { return this->_pbuf.size(); }

		fd_wrapper<fd_type> _fd;
		std::vector<char_type> _gbuf;
		std::vector<char_type> _pbuf;
	};

	template<class Base>
	struct basic_ikernelbuf: public virtual Base {

		using typename Base::int_type;
		using typename Base::traits_type;
		using typename Base::char_type;
		using typename Base::pos_type;
		typedef uint32_t size_type;

		enum struct State {
			READING_SIZE,
			BUFFERING_PAYLOAD,
			READING_PAYLOAD
		};

		static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
			"bad base class for ibasic_kernelbuf");

		int_type underflow() {
			int_type ret = this->Base::underflow();
			this->read_kernel_packetsize();
			this->buffer_payload();
			this->read_payload();
			if (this->_state == State::READING_PAYLOAD) {
				return *this->gptr();
			}
			return traits_type::eof();
		}

		std::streamsize xsgetn(char_type* s, std::streamsize n) {
			if (this->egptr() == this->gptr()) {
				this->Base::underflow();
			}
			if (!this->read_kernel_packetsize()) {
				return std::streamsize(0);
			}
			if (!this->buffer_payload()) {
				return std::streamsize(0);
			}
			return this->Base::xsgetn(s, n);
		}

	private:

		bool read_kernel_packetsize() {
			bool ret = true;
			if (this->state() == State::READING_SIZE) {
				size_type count = this->egptr() - this->gptr();
				if (count >= this->hdrsize()) {
					Bytes<size_type> size(this->gptr(), this->gptr() + this->hdrsize());
					size.to_host_format();
					this->gbump(this->hdrsize());
					this->setpos(this->readoff());
					this->setsize(size);
					this->sets(State::BUFFERING_PAYLOAD);
				} else {
					ret = false;
				}
			}
			this->dumpstate(ret);
			return ret;
		}

		bool buffer_payload() {
			bool ret = true;
			if (this->_state == State::BUFFERING_PAYLOAD) {
				if (this->readend() - this->packetpos() >= this->payloadsize()) {
					char_type* pos = this->eback() + this->packetpos();
					this->setg(this->eback(), pos, pos + this->packetsize());
					this->sets(State::READING_PAYLOAD);
				} else {
					this->setg(this->eback(), this->egptr(), this->egptr());
					ret = false;
				}
			}
			this->dumpstate(ret);
			return ret;
		}

		bool read_payload() {
			bool ret = true;
			if (this->_state == State::READING_PAYLOAD) {
				this->dumpstate(ret);
				pos_type off = this->readoff() + pos_type(1);
				if (off - this->packetpos() == this->payloadsize()) {
					this->sets(State::READING_SIZE);
				}
			}
			return ret;
		}

		void dumpstate(bool ret) {
			std::clog << std::setw(20) << std::left << this->state()
				<< "pbase=" << (void*)this->pbase()
				<< ", pptr=" << (void*)this->pptr()
				<< ", eback=" << (void*)this->eback()
				<< ", gptr=" << (void*)this->gptr()
				<< ", egptr=" << (void*)this->egptr()
				<< ", size=" << this->packetsize()
				<< ", start=" << this->packetpos()
				<< ", ret=" << ret
				<< std::endl;
		}

		friend std::ostream& operator<<(std::ostream& out, State rhs) {
			switch (rhs) {
				case State::READING_SIZE: out << "READING_SIZE"; break;
				case State::BUFFERING_PAYLOAD: out << "BUFFERING_PAYLOAD"; break;
				case State::READING_PAYLOAD: out << "READING_PAYLOAD"; break;
				default: break;
			}
			return out;
		}

		void sets(State rhs) { this->_state = rhs; }
		State state() const { return this->_state; }
		pos_type readoff() {
			return this->seekoff(0, std::ios_base::cur, std::ios_base::in);
		}
		pos_type readend() {
			return this->seekoff(0, std::ios_base::end, std::ios_base::in);
		}
		void setsize(size_type rhs) { this->_packetsize = rhs; }
		size_type packetsize() const { return this->_packetsize; }
		size_type payloadsize() const { return this->_packetsize - this->hdrsize(); }
		static constexpr
		size_type hdrsize() { return sizeof(_packetsize); }
		pos_type packetpos() const { return this->_packetpos; }
		void setpos(pos_type rhs) { this->_packetpos = rhs; }

		size_type _packetsize = 0;
		pos_type _packetpos = 0;
		State _state = State::READING_SIZE;
	};

	template<class Base>
	struct basic_okernelbuf: public virtual Base {

		using typename Base::int_type;
		using typename Base::traits_type;
		using typename Base::char_type;
		using typename Base::pos_type;
		typedef uint32_t size_type;

		enum struct State {
			WRITING_SIZE,
			WRITING_PAYLOAD
		};

		static_assert(std::is_base_of<std::basic_streambuf<char_type>, Base>::value,
			"bad base class for basic_okernelbuf");

		basic_okernelbuf() = default;
		virtual ~basic_okernelbuf() { this->end_packet(); }

		int sync() {
			this->end_packet();
			return this->Base::sync();
		}

		int_type overflow(int_type c) {
			std::clog << "overflow()" << std::endl;
			int_type ret = this->Base::overflow(c);
			this->begin_packet();
			return ret;
		}

		std::streamsize xsputn(const char_type* s, std::streamsize n) {
			std::clog << "xsputn()" << std::endl;
			this->begin_packet();
			return this->Base::xsputn(s, n);
		}

	private:

		void begin_packet() {
			if (this->state() == State::WRITING_SIZE) {
				this->sets(State::WRITING_PAYLOAD);
				this->setbeg(this->writepos());
				this->putsize(0);
				std::clog << "begin_packet()     "
					<< "pbase=" << (void*)this->pbase()
					<< ", pptr=" << (void*)this->pptr()
					<< ", eback=" << (void*)this->eback()
					<< ", gptr=" << (void*)this->gptr()
					<< ", egptr=" << (void*)this->egptr()
					<< std::endl;
			}
		}

		void end_packet() {
			if (this->state() == State::WRITING_PAYLOAD) {
				std::clog << "end_packet()" << std::endl;
				pos_type end = this->writepos();
				size_type s = end - this->_begin;
				if (s == sizeof(size_type)) {
					this->pbump(-static_cast<std::make_signed<size_type>::type>(s));
				} else {
					this->seekpos(this->_begin, std::ios_base::out);
					this->putsize(s);
					this->seekpos(end, std::ios_base::out);
				}
				this->sets(State::WRITING_SIZE);
			}
		}

		void putsize(size_type s) {
			Bytes<size_type> pckt_size(s);
			pckt_size.to_network_format();
			this->Base::xsputn(pckt_size.begin(), pckt_size.size());
		}

		void setbeg(pos_type rhs) { this->_begin = rhs; }
		pos_type writepos() {
			return this->seekoff(0, std::ios_base::cur, std::ios_base::out);
		}

		void sets(State rhs) { this->_state = rhs; }
		State state() const { return this->_state; }

		pos_type _begin = 0;
		State _state = State::WRITING_SIZE;
	};

	template<class Base>
	struct basic_kernelbuf:
		public basic_okernelbuf<Base>,
		public basic_ikernelbuf<Base>
	{
	};

//	template<class T>
//	struct basic_websocketbuf: public basic_fdbuf<T> {
//
//		using typename std::basic_streambuf<T>::int_type;
//		using typename std::basic_streambuf<T>::traits_type;
//		using typename std::basic_streambuf<T>::char_type;
//		using typename basic_fdbuf<T>::fd_type;
//
//		typedef basic_fdbuf<T> basebuf_type;
//
//		enum struct State {
//			INITIAL_STATE,
//			PARSING_HTTP_METHOD,
//			PARSING_HTTP_STATUS,
//			PARSING_HEADERS,
//			PARSING_ERROR,
//			PARSING_SUCCESS,
//			REPLYING_TO_HANDSHAKE,
//			HANDSHAKE_SUCCESS,
//			STARTING_HANDSHAKE,
//			WRITING_HANDSHAKE,
//			READING_FRAME,
//			READING_PAYLOAD,
//			WRITING_FRAME,
//			WRITING_PAYLOAD
//		};
//
//		enum struct Role { SERVER, CLIENT };
//		
//		explicit basic_websocketbuf(std::basic_streambuf<T>* oldbuf):
//			_orig(oldbuf) {}
//		~basic_websocketbuf() { delete this->_orig; }
//
//		basic_websocketbuf(basic_websocketbuf&& rhs):
//			_orig(rhs._orig),
//			_state(rhs._state),
//			_role(rhs._role),
//			_frame(rhs._frame),
//			_nread(rhs._nread)
//			{}
//
//		int_type underflow() {
//			if (this->_orig->gptr() != this->_orig->egptr()) {
//				return traits_type::to_int_type(*this->gptr());
//			}
//			// fill buffer from fd
//			int_type c = this->_orig->sgetc();
//			if (c == traits_type::eof()) {
//				return traits_type::eof();
//			}
//			// initialise ``server'' role
//			if (this->state() == State::INITIAL_STATE) {
//				initserver();
//			}
//			// perform handshake if any
//			handshake();
//			if (this->state() != State::HANDSHAKE_SUCCESS) {
//				return traits_type::eof();
//			}
//			// read frame header
//			if (this->state() == State::READING_FRAME) {
//				size_t hdrsz = _frame.decode_header(this->_orig->gptr(), this->_orig->egptr());
//				if (hdrsz > 0) {
//					Logger<Level::WEBSOCKET>() << "recv header " << this->_frame << std::endl;
//					this->_nread = 0;
//					this->_orig->gbump(hdrsz);
//					this->sets(State::READING_PAYLOAD);
//					if (_frame.opcode() == Opcode::CONN_CLOSE) {
//						Logger<Level::WEBSOCKET>() << "Close frame" << std::endl;
//						return traits_type::eof();
//					}
//					// TODO: validate frame
//				}
//			}
//			// read payload
//			if (this->state() == State::READING_PAYLOAD) {
//				if (this->_orig->gptr() == this->_orig->egptr()) {
//					return traits_type::eof();
//				}
//				if (this->_nread == _frame.payload_size()) {
//					this->sets(State::READING_FRAME);
//					return this->underflow();
//				}
//				char_type ch = _frame.getpayloadc(this->_orig->gptr(), this->_nread);
//				this->_orig->gbump(1);
//				++this->_nread;
//				return traits_type::to_int_type(ch);
//			}
//			return traits_type::eof();
//		}
//
//		int sync() { return this->_orig->sync(); }
//
////		int_type overflow(int_type c = traits_type::eof()) {
////			if (this->state() == State::INITIAL_STATE) {
////				this->initclient();
////			}
////			handshake();
////			if (this->state() != State::HANDSHAKE_SUCCESS) {
////				return traits_type::eof();
////			}
////			if (this->state() == State::WRITING_FRAME) {
////				this->_frame = {};
////				this->_frame.opcode(Opcode::BINARY_FRAME);
////				this->_frame.fin(1);
////				this->_frame.payload_size(input_size);
////				this->_frame.mask(components::rng());
////				this->_frame.encode(result);
////				this->_frame.copy_payload(first, last, result);
////				Logger<Level::WEBSOCKET>() << "send header " << this->frame << std::endl;
////			}
////			websocket_encode(buf, buf + size, std::back_inserter(send_buffer));
////			this->flush();
////			ret = size;
////			return ret;
////		}
//
//		basic_websocketbuf& operator=(basic_websocketbuf&) = delete;
//		basic_websocketbuf(basic_websocketbuf&) = delete;
//
//	private:
//
//		template<class It>
//		void read_header(It first, It last) {
//			switch (this->state()) {
//				case State::PARSING_HTTP_STATUS: parse_http_status(std::string(first, last)); break;
//				case State::PARSING_HTTP_METHOD: parse_http_method(first, last); break;
//				case State::PARSING_HEADERS: parse_http_headers(first, last); break;
//				case State::PARSING_ERROR:
//				case State::PARSING_SUCCESS:
//				case State::HANDSHAKE_SUCCESS:
//				case State::INITIAL_STATE:
//				case State::REPLYING_TO_HANDSHAKE:
//				case State::STARTING_HANDSHAKE:
//				case State::WRITING_HANDSHAKE:
//				default: break;
//			}
//		}
//
//		void parse_http_status(std::string line) {
//			if (line.find("HTTP") == 0) {
//				auto sep1 = line.find(' ');
//				if (sep1 != std::string::npos) {
//					auto sep2 = line.find(' ', sep1);
//					if (sep2 != std::string::npos && line.compare(sep1, sep2-sep1, "101")) {
//						this->sets(State::PARSING_HEADERS);
//						Logger<Level::WEBSOCKET>() << "parsing headers" << std::endl;
//					} else {
//						this->sets(State::PARSING_ERROR);
//						Logger<Level::WEBSOCKET>()
//							<< "bad HTTP status in web socket hand shake" << std::endl;
//					}
//				}
//			} else {
//				this->sets(State::PARSING_ERROR);
//				Logger<Level::WEBSOCKET>()
//					<< "bad method in web socket hand shake" << std::endl;
//			}
//		}
//
//		template<class It>
//		void parse_http_method(It first, It last) {
//			static const char GET[] = "GET";
//			if (std::search(first, last, GET, GET + sizeof(GET)-1) != last) {
//				this->sets(State::PARSING_HEADERS);
//				Logger<Level::WEBSOCKET>() << "parsing headers" << std::endl;
//			} else {
//				this->sets(State::PARSING_ERROR);
//				Logger<Level::WEBSOCKET>()
//					<< "bad method in web socket hand shake" << std::endl;
//			}
//		}
//
//		template<class It>
//		void parse_http_headers(It first, It last) {
//			It it = std::find(first, last, ':');
//			if (it != last) {
//				std::string key(first, it);
//				// advance two characters further (colon plus space)
//				std::string value(it+2, last);
//				if (_http_headers.size() == MAX_HEADERS) {
//					this->sets(State::PARSING_ERROR);
//					Logger<Level::WEBSOCKET>()
//						<< "too many headers in HTTP request" << std::endl;
//				} else if (_http_headers.contain(key.c_str())) {
//					this->sets(State::PARSING_ERROR);
//					Logger<Level::WEBSOCKET>()
//						<< "duplicate HTTP header: '" << key << '\'' << std::endl;
//				} else {
//					this->_http_headers[key] = value;
//					Logger<Level::WEBSOCKET>()
//						<< "Header['" << key << "'] = '" << value << "'" << std::endl;
//				}
//			}
//		}
//
//		bool is_server() const { return this->_role == Role::SERVER; }
//		bool is_client() const { return this->_role == Role::CLIENT; }
//
//		void handshake() {
//			State old_state = this->state();
//			switch (this->state()) {
//				case State::HANDSHAKE_SUCCESS: break;
//				case State::REPLYING_TO_HANDSHAKE:
//					if (this->sync() == 0) {
//						this->sets(State::HANDSHAKE_SUCCESS);
//					}
//					break;
//				case State::PARSING_SUCCESS:
//					if (this->is_server()) this->reply_success(); else this->sets(State::HANDSHAKE_SUCCESS);
//					break;
//				case State::PARSING_ERROR:
//					this->is_server() ? this->reply_error() : this->close();
//					break;
//				case State::STARTING_HANDSHAKE:
//					start_handshake();
//					break;
//				case State::WRITING_HANDSHAKE:
//					if (this->flush()) {
//						this->sets(State::PARSING_HTTP_STATUS);
//					}
//					break;
//				case State::INITIAL_STATE:
//				case State::PARSING_HTTP_METHOD:
//				case State::PARSING_HTTP_STATUS:
//				case State::PARSING_HEADERS:
//					read_http_method_and_headers();
//					break;
//				default: break;
//			}
//			if (old_state != this->state()) {
//				handshake();
//			}
//		}
//
//		bool validate_headers() const {
//			bool ret;
//			if (this->is_client()) {
//				std::string valid_accept(ACCEPT_HEADER_LENGTH, 0);
//				websocket_accept_header(this->_key, valid_accept.begin());
//				ret = _http_headers.contain("Sec-WebSocket-Accept", valid_accept);
//			} else {
//				ret = _http_headers.contain("Sec-WebSocket-Key")
//					&& _http_headers.contain("Sec-WebSocket-Version");
//			}
//			ret = ret 
//				&& _http_headers.contain("Sec-WebSocket-Protocol", "binary")
//				&& _http_headers.contain("Upgrade", "websocket")
//				&& _http_headers.contain_value("Connection", "Upgrade");
//			return ret;
//		}
//
//		void read_http_method_and_headers() {
//			if (this->_orig->gptr() == this->_orig->egptr()) {
//				this->_orig->underflow();
//			}
//			char_type* beg = this->_orig->gptr();
//			char_type* end = this->_orig->egptr();
//			char_type* pos = beg;
//			char_type* found;
//			while ((found = std::search(pos, end,
//				HTTP_FIELD_SEPARATOR, HTTP_FIELD_SEPARATOR + 2)) != end
//				&& pos != found && state != State::PARSING_ERROR)
//			{
//				read_header(pos, found);
//				pos = found + 2;
//			}
//			if (pos == found && pos != beg) {
////			 TODO: is reset() needed here?
////				recv_buffer.reset();
//				if (!validate_headers()) {
//					this->sets(State::PARSING_ERROR);
//					Logger<Level::WEBSOCKET>() << "parsing error" << std::endl;
//				} else {
//					this->sets(State::PARSING_SUCCESS);
//					Logger<Level::WEBSOCKET>() << "parsing success" << std::endl;
//				}
//			}
//		}
//
//		void reply_success() {
//			Logger<Level::WEBSOCKET>() << "replying success" << std::endl;
//			std::string accept_header(ACCEPT_HEADER_LENGTH, 0);
//			websocket_accept_header(_http_headers["Sec-WebSocket-Key"], accept_header.begin());
//			std::stringstream response;
//			response
//				<< "HTTP/1.1 101 Switching Protocols" << HTTP_FIELD_SEPARATOR
//				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
//				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
//				<< "Sec-WebSocket-Accept: " << accept_header << HTTP_FIELD_SEPARATOR
//				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
//				<< HTTP_FIELD_SEPARATOR;
//			std::string buf = response.str();
//			this->_orig->xsputn(buf.data(), buf.size());
//			this->sets(State::REPLYING_TO_HANDSHAKE);
//		}
//
//		void reply_error() {
//			this->_orig->xsputn(BAD_REQUEST, sizeof(BAD_REQUEST)-1);
//			this->sets(State::REPLYING_TO_HANDSHAKE);
//		}
//
//		void start_handshake() {
//			websocket_key(this->_key.begin());
//			std::stringstream request;
//			request
//				<< "GET / HTTP/1.1" << HTTP_FIELD_SEPARATOR
//				<< "User-Agent: Factory/" << VERSION << HTTP_FIELD_SEPARATOR
//				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
//				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
//				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
//				<< "Sec-WebSocket-Version: 13" << HTTP_FIELD_SEPARATOR
//				<< "Sec-WebSocket-Key: " << this->_key << HTTP_FIELD_SEPARATOR
//				<< HTTP_FIELD_SEPARATOR;
//			std::string buf = request.str();
//			this->_orig->sputn(buf.data(), buf.size());
//			this->sets(State::WRITING_HANDSHAKE);
//			Logger<Level::WEBSOCKET>() << "writing handshake " << request.str() << std::endl;
//		}
//
//		void sets(State rhs) {
//			this->_state = rhs;
//			Logger<Level::WEBSOCKET>() << "state=" << this->_state << std::endl;
//		}
//		State state() const { return this->_state; }
//		void setrole(Role rhs) { this->_role = rhs; }
//		State role() const { return this->_role; }
//
//		void initserver() {
//			this->sets(State::PARSING_HTTP_METHOD);
//			this->setrole(Role::SERVER);
//		}
//
//		void initclient() {
//			this->sets(State::STARTING_HANDSHAKE);
//			this->setrole(Role::CLIENT);
//		}
//
//		friend std::ostream& operator<<(std::ostream& out, State rhs) {
//			switch (rhs) {
//				case State::PARSING_HTTP_STATUS: out << "PARSING_HTTP_STATUS"; break;
//				case State::PARSING_HTTP_METHOD: out << "PARSING_HTTP_METHOD"; break;
//				case State::PARSING_HEADERS: out << "PARSING_HTTP_HEADERS"; break;
//				case State::PARSING_ERROR: out << "PARSING_ERROR"; break;
//				case State::PARSING_SUCCESS: out << "PARSING_SUCCESS"; break;
//				case State::HANDSHAKE_SUCCESS: out << "HANDSHAKE_SUCCESS"; break;
//				case State::INITIAL_STATE: out << "INITIAL_STATE"; break;
//				case State::REPLYING_TO_HANDSHAKE: out << "REPLYING_TO_HANDSHAKE"; break;
//				case State::STARTING_HANDSHAKE: out << "STARTING_HANDSHAKE"; break;
//				case State::WRITING_HANDSHAKE: out << "WRITING_HANDSHAKE"; break;
//				default: break;
//			}
//			return out;
//		}
//
//		std::basic_streambuf<T>* _orig;
//		State _state = State::INITIAL_STATE;
//		Role _role = Role::SERVER;
//		Web_socket_frame _frame = {};
//		std::size_t _nread = 0;
//		std::size_t _nwritten = 0;
//
//		HTTP_headers _http_headers;
//		std::string _key;
//
//		static const size_t MAX_HEADERS = 20;
//		static const size_t ACCEPT_HEADER_LENGTH = 29;
//		static const size_t WEBSOCKET_KEY_BASE64_LENGTH = 24;
//
//		constexpr static const char* HTTP_FIELD_SEPARATOR = "\r\n";
//		constexpr static const char* BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\n\r\n";
//	};

	template<class T, class Fd=int>
	struct basic_ifdstream: public std::basic_istream<T> {
		explicit basic_ifdstream(Fd fd): std::basic_istream<T>(new basic_fdbuf<T,Fd>(fd, 512, 0)) {}
		virtual ~basic_ifdstream() { delete this->rdbuf(); }
	};

	template<class T, class Fd=int>
	struct basic_ofdstream: public std::basic_ostream<T> {
		explicit basic_ofdstream(Fd fd): std::basic_ostream<T>(new basic_fdbuf<T,Fd>(fd, 0, 512)) {}
		virtual ~basic_ofdstream() { delete this->rdbuf(); }
	};

	template<class T, class Fd=int>
	struct basic_fd_iostream: public std::basic_iostream<T> {
		explicit basic_fd_iostream(Fd fd): std::basic_iostream<T>(new basic_fdbuf<T,Fd>(fd, 512, 512)) {}
		virtual ~basic_fd_iostream() { delete this->rdbuf(); }
	};

	template<class Base>
	struct basic_kstream: public std::basic_iostream<typename Base::char_type> {
		typedef basic_kernelbuf<Base> kernelbuf;
		typedef typename Base::char_type char_type;
		basic_kstream(): std::basic_iostream<char_type>(new kernelbuf) {}
	};

	typedef basic_fdbuf<char> fdbuf;
	typedef basic_ifdstream<char> ifdstream;
	typedef basic_ofdstream<char> ofdstream;

}
