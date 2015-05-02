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

	struct HTTP_headers {

		size_t size() const { return header.size(); }
		std::string& operator[](const std::string& key) { return header[key]; }

		bool contain(const char* name) const {
			return header.count(name) != 0;
		}

		bool contain(const char* name, const char* value) const {
			auto it = header.find(name);
			return it != header.end() && it->second == value;
		}

		bool contain_value(const char* name, const char* value) const {
			auto it = header.find(name);
			return it != header.end() && it->second.find(value) != std::string::npos;
		}

	private:
		std::unordered_map<std::string, std::string> header;
	};
	
	
	uint32_t ws_send(int sock, const void *buf, size_t len) {
		ssize_t ret = 0;
		ret = ::write(sock, buf, len);
		return ret < 0 ? 0 : static_cast<uint32_t>(ret);
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
	
	template<class It, class Res>
	void encode_hybi(It first, It last, Res result) {

		static_assert(std::is_same<typename std::iterator_traits<Res>::iterator_category,
			std::output_iterator_tag>::value
			&& std::is_same<typename std::iterator_traits<It>::iterator_category,
			std::random_access_iterator_tag>::value, "encode_hybi is defined for input and output iterators only");

		size_t srclength = last - first;
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
	
//		buffer.write(bytes.begin(), offset);
//		buffer.write(first, srclength);
		std::copy(bytes.begin(), bytes.begin() + offset, result);
		std::copy(first, last, result);
//		std::copy(src, src + srclength, target + offset);
	}

	template<class It, class Res>
	size_t decode_hybi2(It first, It last,
					Res output,
					Opcode* opcode)
	{
		size_t srclength = first - last;
		static const size_t MASK_SIZE = 4;
		size_t header_len = 2;
		if (srclength < header_len) return 0;
		Bytes<Web_socket_frame_header> raw_hdr(first, first + header_len);
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
				first + header_len - MASK_SIZE, first + header_len);
			size_t i = 0;
			std::transform(first + header_len, first + header_len + hdr.len, output,
				[&i,&mask] (char ch) {
					ch ^= mask[i%4]; ++i; return ch;
				}
			);
//			std::for_each(first + header_len, first + header_len + hdr.len,
//				[&i,&mask] (char& ch) { ch ^= mask[i%4]; ++i; }
//			);
//			output.write(first + header_len, hdr.len);
		} else {
			std::copy(first + header_len, first + header_len + hdr.len, output);
//			output.write(first + header_len, hdr.len);
		}
		return header_len + hdr.len;
	}
	
	static void gen_sha1(const std::string& web_socket_key, char *target) {
	
		static const size_t HYBI10_ACCEPTHDRLEN = 29;
		static const char HYBI_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
		Bytes<Bytes<unsigned, unsigned char>[5], unsigned char> hash2;
		SHA1 sha1;
		sha1.input(web_socket_key.begin(), web_socket_key.end());
		sha1.input(HYBI_GUID, HYBI_GUID + sizeof(HYBI_GUID)-1);
		sha1.result(hash2.value());
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
	
		base64_encode(hash2.begin(), hash2.end(), target);
//		b64_ntop(hash2, sizeof(hash2), target, HYBI10_ACCEPTHDRLEN);
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
			_http_headers(),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE)
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
						if (_http_headers.size() == MAX_HEADERS) {
							state = State::PARSING_ERROR;
							Logger(Level::WEBSOCKET)
								<< "too many headers in HTTP request" << std::endl;
						} else if (_http_headers.contain(key.c_str())) {
							state = State::PARSING_ERROR;
							Logger(Level::WEBSOCKET)
								<< "duplicate HTTP header: '" << key << '\'' << std::endl;
						} else {
							_http_headers[key] = value;
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

		void do_handshake(int sock) {
			if (state == State::HANDSHAKE_SUCCESS) return;
			read_http_method_and_headers(sock);
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

	private:

		void read_http_method_and_headers(int sock) {
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
			gen_sha1(_http_headers["Sec-WebSocket-Key"], sha1);
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
				encode_hybi(buf, buf + size, std::back_inserter(send_buffer));
				send_buffer.flush(Socket(_socket));
				if (send_buffer.empty()) {
					ret = size;
				}
			}
			return ret;
		}

		uint32_t read(int _socket, char* buf, size_t size) {
			uint32_t bytes_read = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				do_handshake(_socket);
			} else {
				Opcode opcode = Opcode::CONT_FRAME;
				if (mid_buffer.empty()) {
					recv_buffer.fill(Socket(_socket));
					size_t len = decode_hybi2(recv_buffer.read_begin(),
									  recv_buffer.read_end(),
									  std::back_inserter(mid_buffer),
									  &opcode);
					recv_buffer.ignore(len);
				}
				if (opcode == Opcode::CONN_CLOSE) {
					Logger(Level::WEBSOCKET) << "Close frame" << std::endl;
				} else {
					bytes_read = mid_buffer.read(buf, size);
				}
			}
			return bytes_read;
		}
	
	private:
		std::string buffer;
		size_t buffer_offset = 0;
		State state = State::PARSING_HTTP_METHOD;
		HTTP_headers _http_headers;

		LBuffer<char> send_buffer;
		LBuffer<char> recv_buffer;
		LBuffer<char> mid_buffer;

		static const size_t BUFFER_SIZE = 1024;
		static const size_t MAX_HEADERS = 20;
		static const char* HTTP_FIELD_SEPARATOR;
	};

	const char* Handshake::HTTP_FIELD_SEPARATOR = "\r\n";

	struct Web_socket: public Socket {

		Web_socket():
			_handshaker()
			{}

		explicit Web_socket(const Socket& rhs):
			Socket(rhs),
			_handshaker()
			{}

		virtual ~Web_socket() {
			this->close();
		}

		Web_socket& operator=(const Socket& rhs) {
			Socket::operator=(rhs);
			return *this;
		}


		uint32_t read(char* buf, size_t size) {
			return _handshaker.read(_socket, buf, size);
		}

		uint32_t write(const char* buf, size_t size) {
			return _handshaker.write(_socket, buf, size);
		}

		friend std::ostream& operator<<(std::ostream& out, const Web_socket& rhs) {
			return out << rhs._socket;
		}

	private:
		Handshake _handshaker;
	};

}
