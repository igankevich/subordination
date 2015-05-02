namespace factory {

	struct Socket {

		typedef int Flag;
		typedef int Option;

		static const int DEFAULT_FLAGS = SOCK_NONBLOCK | SOCK_CLOEXEC;

		Socket(): _socket(0) {}
		Socket(int socket): _socket(socket) {}
		Socket(const Socket& rhs): _socket(rhs._socket) {}
		Socket(Socket&& rhs): _socket(rhs._socket) { rhs._socket = -1; }
		virtual ~Socket() {}

		void create_socket_if_necessary() {
			if (_socket <= 0) {
				check("socket()", _socket = ::socket(AF_INET, SOCK_STREAM | DEFAULT_FLAGS, 0));
			}
		}

		void bind(Endpoint e) {
			create_socket_if_necessary();
			options(SO_REUSEADDR);
			check("bind()", ::bind(_socket, e.sockaddr(), sizeof(sockaddr_in)));
			Logger(Level::COMPONENT) << "Binding to " << e << std::endl;
		}
		
		void listen() {
			check("listen()", ::listen(_socket, SOMAXCONN));
			Logger(Level::COMPONENT) << "Listening on " << name() << std::endl;
		}

		void connect(Endpoint e) {
			try {
				create_socket_if_necessary();
				check_connect("connect()", ::connect(_socket, e.sockaddr(), sizeof(sockaddr_in)));
				Logger(Level::COMPONENT) << "Connecting to " << e << std::endl;
			} catch (std::system_error& err) {
				Logger(Level::COMPONENT) << "Rethrowing connection error." << std::endl;
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
			Logger(Level::COMPONENT) << "Accepted connection from " << addr << std::endl;
			return std::make_pair(socket, addr);
		}

		void close() {
			if (_socket > 0) {
				Logger(Level::COMPONENT) << "Closing socket " << _socket << std::endl;
				::shutdown(_socket, SHUT_RDWR);
				::close(_socket);
//				check("close()", ::close(_socket));
			}
			_socket = -1;
		}

		void no_reading() {
			if (_socket > 0) {
				check("no_reading()", ::shutdown(_socket, SHUT_RD));
			}
		}

		int fd() const { return _socket; }

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

	protected:
		int _socket;
	};

}
namespace factory {

	struct Server_socket: public Socket {
		Server_socket() {}
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

	const size_t BUFSIZE = 65536;
	const size_t DBUFSIZE = (BUFSIZE * 3) / 4 - 20;

	typedef struct {
		char	  *cin_buf;
		char	  *cout_buf;
		char	  *tin_buf;
		char	  *tout_buf;
		std::unordered_map<std::string, std::string> header;

		bool header_is_present(const char* name) const {
			return header.count(name) != 0;
		}

		bool header_equals(const char* name, const char* value) const {
			auto it = header.find(name);
			return it != header.end() && it->second == value;
		}

		bool header_contains(const char* name, const char* value) const {
			auto it = header.find(name);
			return it != header.end() && it->second.find(value) != std::string::npos;
		}

	} ws_ctx_t;
	
	
	int b64_ntop(const unsigned char* src, size_t srclength, char *target, size_t targsize) {
		size_t encoded_size = factory::base64_encoded_size(srclength);
		if (encoded_size > targsize) return -1;
		factory::base64_encode(src, src + srclength, target);
		return encoded_size;
	}
	
	uint32_t ws_send(int sock, const void *buf, size_t len) {
		ssize_t ret = 0;
		ret = ::write(sock, buf, len);
		return ret < 0 ? 0 : static_cast<uint32_t>(ret);
	}
	
	ws_ctx_t *alloc_ws_ctx() {
		ws_ctx_t *ctx;
		ctx = new ws_ctx_t;
		ctx->cin_buf = new char[BUFSIZE];
		ctx->cout_buf = new char[BUFSIZE];
		ctx->tin_buf = new char[BUFSIZE];
		ctx->tout_buf = new char[BUFSIZE];
		return ctx;
	}
	
	enum struct Opcode: int8_t {
		CONT_FRAME   = 0x0,
		TEXT_FRAME   = 0x1,
		BINARY_FRAME = 0x2,
		CONN_CLOSE   = 0x8,
		PING		 = 0x9,
		PONG		 = 0xa
	};
	
	struct Web_socket_frame_header {
		uint16_t extlen : 16;
		uint16_t len	: 7;
		uint16_t mask   : 1;
		uint16_t opcode : 4;
		uint16_t rsv3   : 1;
		uint16_t rsv2   : 1;
		uint16_t rsv1   : 1;
		uint16_t fin	: 1;

		bool masked() const { return mask == 1; }
		bool is_binary_frame() const {
			return static_cast<Opcode>(opcode) == Opcode::BINARY_FRAME;
		}
		bool has_valid_opcode() const { return opcode >= 0x0 && opcode <= 0xf; }

		friend std::ostream& operator<<(std::ostream& out, const Web_socket_frame_header& rhs) {
			return out
				<< "fin=" << rhs.fin << ','
				<< "rsv1=" << rhs.rsv1 << ','
				<< "rsv2=" << rhs.rsv2 << ','
				<< "rsv3=" << rhs.rsv3 << ','
				<< "opcode=" << rhs.opcode << ','
				<< "mask=" << rhs.mask << ','
				<< "len=" << rhs.len << ','
				<< "extlen=" << rhs.extlen;
		}
	};
	
	int encode_hybi(const char* src, size_t srclength, LBuffer<char>& buffer) {
		if (srclength == 0) return 0;
		// TODO: mask data with random key
		Web_socket_frame_header hdr = { 0 };
		size_t offset = 2;
		hdr.opcode = static_cast<uint16_t>(Opcode::BINARY_FRAME);
		hdr.fin = 1;
		if (srclength <= 125) {
			hdr.len = srclength;
		} else if (srclength > 125 && srclength < 65536) {
			hdr.len = 126;
			hdr.extlen = srclength;
//			Bytes<uint16_t> raw = srclength;
//			raw.to_network_format();
//			std::copy(raw.begin(), raw.end(), target + offset);
			offset += 2;
		} else {
			hdr.len = 127;
			// TODO: ???
//			Bytes<uint64_t> raw = srclength;
//			raw.to_network_format();
//			std::copy(raw.begin(), raw.end(), target + offset);
//			offset += 8;
		}
	
		Bytes<Web_socket_frame_header> bytes = hdr;
		bytes.to_network_format();
//		std::copy(bytes.begin(), bytes.begin() + offset, target);
	
//		Logger(Level::WEBSOCKET)
//			<< "assertions: " << std::boolalpha
//			<< (target[0] == (char)((static_cast<int>(opcode) & 0x0F) | 0x80))
//			<< (target[1] == (char)srclength)
//			<< std::endl;
//	
//		Logger(Level::WEBSOCKET)
//			<< "Header: "
//			<< (uint32_t)target[0] << ' ' << (uint32_t)target[1] << ' '
//			<< (uint32_t)bytes[0] << ' ' << (uint32_t)hdr.fin << ' '
//			<< (uint32_t)(unsigned char)((static_cast<uint32_t>(opcode) & 0x0F) | 0x80)
//			<< ' ' << (uint32_t)(char)srclength << ' '
//			<< ' ' << sizeof(hdr) << ' '
//			<< std::endl;
	
		buffer.write(bytes.begin(), offset);
		buffer.write(src, srclength);
//		std::copy(src, src + srclength, target + offset);
		int ret = offset + srclength;
		
		return ret;
	}

	int decode_hybi2(char *src, size_t srclength,
					char *target,
					Opcode* opcode)
	{
		static const size_t MASK_SIZE = 4;
		size_t header_len = 2;
		if (srclength < header_len) return 0;
		Bytes<Web_socket_frame_header> raw_hdr(src, src + header_len);
		raw_hdr.to_host_format();
		Web_socket_frame_header hdr = raw_hdr;
		Logger(Level::WEBSOCKET) << "recv header " << hdr << std::endl;
		*opcode = static_cast<Opcode>(hdr.opcode);
		if (hdr.masked()) header_len += MASK_SIZE;
		if (srclength < hdr.len + header_len
			|| !hdr.has_valid_opcode()
			|| !hdr.is_binary_frame())
		{
			return 0;
		}
		if (hdr.masked()) {
			Bytes<char[MASK_SIZE]> mask(
				src + header_len - MASK_SIZE, src + header_len);
			size_t i = 0;
			std::transform(src + header_len, src + header_len + hdr.len, target,
				[&i,&mask] (char ch) {
					ch ^= mask[i%4]; ++i; return ch;
				}
			);
		} else {
			std::copy(src + header_len, src + header_len + hdr.len, target);
		}
		return hdr.len;
	}
	
	static void gen_sha1(const std::string& web_socket_key, char *target) {
	
		static const size_t HYBI10_ACCEPTHDRLEN = 29;
		static const char HYBI_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
		Bytes<Bytes<unsigned, unsigned char>[5], unsigned char> hash2;
		SHA1 sha1;
		sha1.Input(web_socket_key.data(), web_socket_key.size());
		sha1.Input(HYBI_GUID, sizeof(HYBI_GUID)-1);
		sha1.Result(hash2.value());
		std::for_each(hash2.value(), hash2.value() + 5,
			std::mem_fun_ref(&Bytes<unsigned, unsigned char>::to_network_format));

//		auto ret = std::mismatch(hash, hash + SHA_DIGEST_LENGTH, hash2.begin());
//		if (ret.first == hash + SHA_DIGEST_LENGTH) {
//			Logger(Level::WEBSOCKET) << "SHA1 match" << std::endl;
//		} else {
//			Logger(Level::WEBSOCKET) << "SHA1 do not match: "
//				<< Bytes<char[SHA_DIGEST_LENGTH]>(hash, hash + SHA_DIGEST_LENGTH)
//				<< " /= "
//				<< hash2
//				<< std::endl;
//			Logger(Level::WEBSOCKET) << "Mismatching elem: "
//				<< Bytes<char>(*ret.first)
//				<< " /= "
//				<< Bytes<char>(*ret.second)
//				<< std::endl;
//		}
	
		b64_ntop(hash2, sizeof(hash2), target, HYBI10_ACCEPTHDRLEN);
//		b64_ntop(hash, sizeof hash, target, HYBI10_ACCEPTHDRLEN);
		target[HYBI10_ACCEPTHDRLEN-1] = 0;
	}


	struct Handshake {

		enum struct State: char {
			PARSING_HTTP_METHOD,
			PARSING_HEADERS,
			PARSING_ERROR,
			HANDSHAKE_SUCCESS
		};

		Handshake():
			buffer(BUFFER_SIZE, 0),
			_context(alloc_ws_ctx()),
			send_buffer(BUFSIZE)
		{}

		template<class It>
		void read_header(It first, It last) {
			switch (state) {
				case State::PARSING_HTTP_METHOD: {
					std::string line(first, last);
					if (line.find("GET") == 0) {
						state = State::PARSING_HEADERS;
						Logger(Level::WEBSOCKET) << "parsing headers" << std::endl;
					} else {
						state = State::PARSING_ERROR;
						Logger(Level::WEBSOCKET)
							<< "bad method in web socket hand shake" << std::endl;
					}
					} break;
				case State::PARSING_HEADERS: {
					It it = std::find(first, last, ':');
					if (it != last) {
						std::string key(first, it);
						// advance two characters further (colon plus space)
						std::string value(it+2, last);
						if (_context->header.size() == MAX_HEADERS) {
							state = State::PARSING_ERROR;
							Logger(Level::WEBSOCKET)
								<< "too many headers in HTTP request" << std::endl;
						} else if (_context->header.find(key) != _context->header.end()) {
							state = State::PARSING_ERROR;
							Logger(Level::WEBSOCKET)
								<< "duplicate HTTP header: '" << key << '\'' << std::endl;
						} else {
							_context->header[key] = value;
							Logger(Level::WEBSOCKET)
								<< "Header['" << key << "'] = '" << value << "'" << std::endl;
						}
					}
					} break;
				case State::PARSING_ERROR:
				case State::HANDSHAKE_SUCCESS:
				default: break;
			}
		}

		ws_ctx_t *do_handshake(int sock) {
			if (state == State::HANDSHAKE_SUCCESS) return _context;
			read_method_and_headers(sock);
			switch (state) {
				case State::HANDSHAKE_SUCCESS:
					reply_success(sock);
					break;
				case State::PARSING_ERROR:
					reply_error(sock);
					break;
				case State::PARSING_HTTP_METHOD:
				case State::PARSING_HEADERS:
				default:
					break;
			}
			return _context;
		}

		bool success() const { return state == State::HANDSHAKE_SUCCESS; }

		bool validate_headers() {
			return
				_context->header_is_present("Sec-WebSocket-Key")
				&& _context->header_is_present("Sec-WebSocket-Version")
				&& _context->header_equals("Sec-WebSocket-Protocol", "binary")
				&& _context->header_equals("Upgrade", "websocket")
				&& _context->header_contains("Connection", "Upgrade");
		}

	private:

		void read_method_and_headers(int sock) {
			ssize_t len = ::read(sock, &buffer[buffer_offset], BUFFER_SIZE);
			size_t pos = 0;
			size_t found;
			while ((found = buffer.find(HTTP_FIELD_SEPARATOR, pos)) != std::string::npos
				&& pos != found && found < buffer_offset + len
				&& state != State::PARSING_ERROR)
			{
				read_header(buffer.begin() + pos, buffer.begin() + found);
				pos = found + 2;
			}
			if (pos == found) {
				if (!validate_headers()) {
					state = State::PARSING_ERROR;
					Logger(Level::WEBSOCKET) << "parsing error" << std::endl;
				} else {
					state = State::HANDSHAKE_SUCCESS;
					Logger(Level::WEBSOCKET) << "parsing success" << std::endl;
				}
				buffer_offset = 0;
			} else {
				std::copy(buffer.data() + pos, buffer.data() + found, &buffer[0]);
				buffer_offset = found - pos;
			}
		}

		void reply_success(int sock) {
			char sha1[29];
			gen_sha1(_context->header["Sec-WebSocket-Key"], sha1);
			std::stringstream response;
			response
				<< "HTTP/1.1 101 Switching Protocols" << HTTP_FIELD_SEPARATOR
				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Accept: " << sha1 << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
				<< HTTP_FIELD_SEPARATOR;
			std::string buf = response.str();
			ws_send(sock, buf.data(), buf.size());
		}

		void reply_error(int sock) {
			std::string resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			ws_send(sock, resp.data(), resp.size());
		}

	public:
		uint32_t write(int _socket, const char* buf, size_t size) {
			uint32_t ret = 0;
			if (state == State::HANDSHAKE_SUCCESS) {
				encode_hybi(buf, size, send_buffer);
				send_buffer.flush(Socket(_socket));
				if (send_buffer.empty()) {
					ret = size;
				}
			}
			return ret;
		}
	
	private:
		std::string buffer;
		size_t buffer_offset = 0;
		State state = State::PARSING_HTTP_METHOD;
		ws_ctx_t * _context;

		LBuffer<char> send_buffer;

		static const size_t BUFFER_SIZE = 1024;
		static const size_t MAX_HEADERS = 20;
		static const char* HTTP_FIELD_SEPARATOR;
	};

	const char* Handshake::HTTP_FIELD_SEPARATOR = "\r\n";

	struct Web_socket: public Socket {

		enum State {
			HANDSHAKE,
			NORMAL
		};

		Web_socket(): _context(nullptr), _handshaker() {}
		explicit Web_socket(const Socket& rhs): Socket(rhs), _context(nullptr), _handshaker() {}

		virtual ~Web_socket() {
			this->close();
			if (_context) {
				delete[] _context->cin_buf;
				delete[] _context->cout_buf;
				delete[] _context->tin_buf;
				delete[] _context->tout_buf;
				delete _context;
			}
		}

		Web_socket& operator=(const Socket& rhs) {
			Socket::operator=(rhs);
			return *this;
		}

		uint32_t read(char* buf, size_t size) {
			if (!_context) {
				_context = _handshaker.do_handshake(_socket);
			} else {
				{
					unsigned int cnt = std::min(tout_end-tout_start, (unsigned int)size);
					if (cnt > 0) {
						std::copy(_context->tout_buf + tout_start,
							_context->tout_buf + tout_start + cnt, buf);
						tout_start += cnt;
						if (tout_start == tout_end) {
							tout_start = 0;
							tout_end = 0;
						}
						return cnt;
					}
				}
				ssize_t bytes = ::read(_socket, _context->tin_buf + tin_end, BUFSIZE-1);
				int len = 0;
				unsigned int left = 0;
				Opcode opcode = Opcode::CONT_FRAME;
				if (bytes <= 0) {
					Logger(Level::WEBSOCKET)
						<< "Nothing to read: "
						<< strerror(errno)
						<< ", tin_start = " << tin_start
						<< ", tin_end = " << tin_end
						<< ", tout_start = " << tout_start
						<< ", tout_end = " << tout_end
						<< std::endl;
					return 0;
				}
				tin_end += bytes;
				len = decode_hybi2(_context->tin_buf + tin_start,
								  tin_end-tin_start,
								  _context->tout_buf + tout_end,
								  &opcode);
				if (opcode == Opcode::CONN_CLOSE || len < 0) {
					Logger(Level::WEBSOCKET) << "Close frame" << std::endl;
					return 0;
				}
				tin_start = 0;
				tin_end = 0;
				tout_end += len;
				unsigned int cnt = std::min(tout_end-tout_start, (unsigned int)size);
				std::copy(_context->tout_buf + tout_start,
					_context->tout_buf + tout_start + cnt, buf);
				tout_start += cnt;
				if (tout_start == tout_end) {
					tout_start = 0;
					tout_end = 0;
				}
				Logger log(Level::WEBSOCKET);
				log << "Message ";
				log << std::hex << std::setfill('0');
				for (int i=0; i< bytes; i++) {
					log << std::setw(2)
						<< (unsigned int)(unsigned char)(_context->tin_buf[i]);
				}
				log << std::endl;
				return cnt;
			}
			return 0;
		}

		uint32_t write(const char* buf, size_t size) {
			return _handshaker.write(_socket, buf, size);
		}

		friend std::ostream& operator<<(std::ostream& out, const Web_socket& rhs) {
			return out << rhs._socket;
		}

	private:
		ws_ctx_t* _context;
		Handshake _handshaker;
		unsigned int tin_start = 0;
		unsigned int tin_end = 0;
		unsigned int tout_start = 0;
		unsigned int tout_end = 0;
	};

}
