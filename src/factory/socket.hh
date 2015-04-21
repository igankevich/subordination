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

	struct Autoinitialize_openssl_library {
		Autoinitialize_openssl_library() {
			::SSL_library_init();
			::OpenSSL_add_all_algorithms();
			::SSL_load_error_strings();
		}
	} _autoinitialize_openssl_library;


	const size_t BUFSIZE = 65536;
	const size_t DBUFSIZE = (BUFSIZE * 3) / 4 - 20;

#define SERVER_HANDSHAKE_HYBI "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
Sec-WebSocket-Protocol: %s\r\n\
\r\n"

#define POLICY_RESPONSE "<cross-domain-policy><allow-access-from domain=\"*\" to-ports=\"*\" /></cross-domain-policy>\n"

	enum Subprotocol {
		BINARY = 0,
		BASE64 = 1
	};
	
	const char* SUBPROTOCOL_NAMES[] = {
		"binary",
		"base64"
	};
	
	typedef struct {
	    char path[1024+1];
	    char host[1024+1];
	    char origin[1024+1];
	    char version[1024+1];
	    char connection[1024+1];
	    char protocols[1024+1];
	    char key1[1024+1];
	    char key2[1024+1];
	    char key3[8+1];
	} headers_t;
	
	typedef struct {
	    int        sockfd;
	    ::SSL_CTX   *ssl_ctx;
	    ::SSL       *ssl;
	    int        hybi;
		Subprotocol subproto;
	    headers_t *headers;
	    char      *cin_buf;
	    char      *cout_buf;
	    char      *tin_buf;
	    char      *tout_buf;
	} ws_ctx_t;
	
	typedef struct {
	    int verbose;
	    char listen_host[256];
	    int listen_port;
	    void (*handler)(ws_ctx_t*);
	    int handler_id;
	    char *cert;
	    char *key;
	    int ssl_only;
	    int daemon;
	    int run_once;
	} settings_t;
	
	int b64_ntop(const unsigned char* src, size_t srclength, char *target, size_t targsize) {
		size_t encoded_size = factory::base64_encoded_size(srclength);
		if (encoded_size > targsize) return -1;
		factory::base64_encode(src, src + srclength, target);
		return encoded_size;
	}
	
	//int b64_pton(const char *src, unsigned char* target, size_t targsize, size_t srclength) {
	//	size_t max_size = factory::base64_max_decoded_size(srclength);
	//	if (max_size > targsize) return -1;
	//	return factory::base64_decode(src, src + srclength, target);
	//}
	
	
	/*
	 * Global state
	 *
	 *   Warning: not thread safe
	 */
	settings_t settings;
	
	
	
	
	/*
	 * SSL Wrapper Code
	 */
	
	ssize_t ws_recv(ws_ctx_t *ctx, void *buf, size_t len) {
	    if (ctx->ssl) {
	        //Logger(Level::WEBSOCKET) << "SSL recv" << std::endl;
	        return ::SSL_read(ctx->ssl, buf, len);
	    } else {
	        return ::read(ctx->sockfd, buf, len);
	    }
	}
	
	uint32_t ws_send(ws_ctx_t *ctx, const void *buf, size_t len) {
		ssize_t ret = 0;
	    if (ctx->ssl) {
	        //Logger(Level::WEBSOCKET) << "SSL send" << std::endl;
	        ret = ::SSL_write(ctx->ssl, buf, len);
	    } else {
	        ret = ::write(ctx->sockfd, buf, len);
	    }
		return ret < 0 ? 0 : static_cast<uint32_t>(ret);
	}
	
	ws_ctx_t *alloc_ws_ctx() {
	    ws_ctx_t *ctx;
		ctx = new ws_ctx_t;
		ctx->cin_buf = new char[BUFSIZE];
		ctx->cout_buf = new char[BUFSIZE];
		ctx->tin_buf = new char[BUFSIZE];
		ctx->tout_buf = new char[BUFSIZE];
	    ctx->headers = new headers_t;
	    ctx->ssl = NULL;
	    ctx->ssl_ctx = NULL;
	    return ctx;
	}
	
	void ws_socket(ws_ctx_t *ctx, int socket) {
	    ctx->sockfd = socket;
	}
	
	ws_ctx_t *ws_socket_ssl(ws_ctx_t *ctx, int socket, char * certfile, char * keyfile) {
	
	    int ret;
	    char* use_keyfile;
	    ws_socket(ctx, socket);
	
	    if (keyfile && (keyfile[0] != '\0')) {
	        // Separate key file
	        use_keyfile = keyfile;
	    } else {
	        // Combined key and cert file
	        use_keyfile = certfile;
	    }
	
	    ctx->ssl_ctx = ::SSL_CTX_new(TLSv1_server_method());
	    if (ctx->ssl_ctx == NULL) {
	        ::ERR_print_errors_fp(stderr);
	        throw std::runtime_error("Failed to configure SSL context");
	    }
	
	    if (::SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, use_keyfile,
	                                    SSL_FILETYPE_PEM) <= 0) {
			std::stringstream msg;
	        msg << "Unable to load private key file '" << use_keyfile << "'." << std::endl;
	        throw std::runtime_error(msg.str());
	    }
	
	    if (::SSL_CTX_use_certificate_file(ctx->ssl_ctx, certfile,
	                                     SSL_FILETYPE_PEM) <= 0) {
			std::stringstream msg;
	        msg << "Unable to load certificate file '" << certfile << "'." << std::endl;
	        throw std::runtime_error(msg.str());
	    }
	
	//    if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, "DEFAULT") != 1) {
	//        sprintf(msg, "Unable to set cipher" << std::endl;
	//        throw std::runtime_error(msg);
	//    }
	
	    // Associate socket and ssl object
	    ctx->ssl = ::SSL_new(ctx->ssl_ctx);
	    ::SSL_set_fd(ctx->ssl, socket);
	
	    ret = ::SSL_accept(ctx->ssl);
	    if (ret < 0) {
	        ::ERR_print_errors_fp(stderr);
	        return NULL;
	    }
	
	    return ctx;
	}
	
	void ws_socket_free(ws_ctx_t *ctx) {
	    if (ctx->ssl) {
	        ::SSL_free(ctx->ssl);
	        ctx->ssl = NULL;
	    }
	    if (ctx->ssl_ctx) {
	        ::SSL_CTX_free(ctx->ssl_ctx);
	        ctx->ssl_ctx = NULL;
	    }
	    if (ctx->sockfd) {
	        shutdown(ctx->sockfd, SHUT_RDWR);
	        close(ctx->sockfd);
	        ctx->sockfd = 0;
	    }
	}
	
	enum struct Opcode: int8_t {
		CONT_FRAME   = 0x0,
		TEXT_FRAME   = 0x1,
		BINARY_FRAME = 0x2,
		CONN_CLOSE   = 0x8,
		PING         = 0x9,
		PONG         = 0xa
	};
	
	struct Web_socket_frame_header {
		uint16_t extlen : 16;
		uint16_t len    : 7;
		uint16_t mask   : 1;
		uint16_t opcode : 4;
		uint16_t rsv3   : 1;
		uint16_t rsv2   : 1;
		uint16_t rsv1   : 1;
		uint16_t fin    : 1;
	};
	
	int encode_hybi(u_char const *src, size_t srclength,
	                char *target, size_t targsize, Opcode opcode)
	{
	    if (srclength == 0) { return 0; }
		Web_socket_frame_header hdr = { 0 };
		size_t offset = 2;
		hdr.opcode = static_cast<uint16_t>(opcode);
		hdr.fin = 1;
	    if (srclength <= 125) {
			hdr.len = srclength;
		} else if (srclength > 125 && srclength < 65536) {
			hdr.len = 126;
			Bytes<uint16_t> raw = srclength;
			raw.to_network_format();
			std::copy(raw.begin(), raw.end(), target + offset);
			offset += 2;
		} else {
			hdr.len = 127;
			Bytes<uint64_t> raw = srclength;
			raw.to_network_format();
			std::copy(raw.begin(), raw.end(), target + offset);
			offset += 8;
		}
	
		Bytes<Web_socket_frame_header> bytes = hdr;
		bytes.to_network_format();
		std::copy(bytes.begin(), bytes.begin() + offset, target);
	
		Logger(Level::WEBSOCKET)
			<< "assertions: " << std::boolalpha
			<< (target[0] == (char)((static_cast<int>(opcode) & 0x0F) | 0x80))
			<< (target[1] == (char)srclength)
			<< std::endl;
	
		Logger(Level::WEBSOCKET)
			<< "Header: "
			<< (uint32_t)target[0] << ' ' << (uint32_t)target[1] << ' '
			<< (uint32_t)bytes[0] << ' ' << (uint32_t)hdr.fin << ' '
			<< (uint32_t)(unsigned char)((static_cast<uint32_t>(opcode) & 0x0F) | 0x80)
			<< ' ' << (uint32_t)(char)srclength << ' '
			<< ' ' << sizeof(hdr) << ' '
			<< std::endl;
	
		int ret;
		if (opcode == Opcode::TEXT_FRAME) {
			ret = b64_ntop(src, srclength, target+offset, targsize-offset);
			ret = ret < 0 ? ret : (ret + offset);
		} else {
			std::copy(src, src + srclength, target + offset);
			ret = offset + srclength;
		}
	    
	    return ret;
	}
	
	int decode_hybi(unsigned char *src, size_t srclength,
	                u_char *target, size_t,
	                unsigned int *opcode, unsigned int *left)
	{
	    unsigned char *frame, *mask, *payload, save_char;
	    char cntstr[4];
	    int masked = 0;
	    int len, framecount = 0;
		unsigned int i = 0;
	    size_t remaining = 0;
	    unsigned int target_offset = 0, hdr_length = 0, payload_length = 0;
	    
	    *left = srclength;
	    frame = src;
	
	    //printf("Deocde new frame" << std::endl;
	    while (1) {
	        // Need at least two bytes of the header
	        // Find beginning of next frame. First time hdr_length, masked and
	        // payload_length are zero
	        frame += hdr_length + 4*masked + payload_length;
	        //printf("frame[0..3]: 0x%x 0x%x 0x%x 0x%x (tot: %d)\n",
	        //       (unsigned char) frame[0],
	        //       (unsigned char) frame[1],
	        //       (unsigned char) frame[2],
	        //       (unsigned char) frame[3], srclength);
	
	        if (frame > src + srclength) {
	            //printf("Truncated frame from client, need %d more bytes\n", frame - (src + srclength) );
	            break;
	        }
	        remaining = (src + srclength) - frame;
	        if (remaining < 2) {
	            //printf("Truncated frame header from client" << std::endl;
	            break;
	        }
	        framecount ++;
	
	        *opcode = frame[0] & 0x0f;
	        masked = (frame[1] & 0x80) >> 7;
	
	        if (*opcode == 0x8) {
	            // client sent orderly close frame
	            break;
	        }
	
	        payload_length = frame[1] & 0x7f;
	        if (payload_length < 126) {
	            hdr_length = 2;
	            //frame += 2 * sizeof(char);
	        } else if (payload_length == 126) {
	            payload_length = (frame[2] << 8) + frame[3];
	            hdr_length = 4;
	        } else {
	            Logger(Level::WEBSOCKET) << "Receiving frames larger than 65535 bytes not supported" << std::endl;
	            return -1;
	        }
	        if ((hdr_length + 4*masked + payload_length) > remaining) {
	            continue;
	        }
	        //printf("    payload_length: %u, raw remaining: %u\n", payload_length, remaining);
	        payload = frame + hdr_length + 4*masked;
	
	        if (*opcode != 1 && *opcode != 2) {
	            Logger(Level::WEBSOCKET) << "Ignoring non-data frame, opcode 0x"
					<< std::hex << *opcode << std::dec << std::endl;
	            continue;
	        }
	
	        if (payload_length == 0) {
	            Logger(Level::WEBSOCKET) << "Ignoring empty frame" << std::endl;
	            continue;
	        }
	
	        if ((payload_length > 0) && (!masked)) {
	            Logger(Level::WEBSOCKET) << "Received unmasked payload from client" << std::endl;
	            return -1;
	        }
	
	        // Terminate with a null for base64 decode
	        save_char = payload[payload_length];
	        payload[payload_length] = '\0';
	
	        // unmask the data
	        mask = payload - 4;
	        for (i = 0; i < payload_length; i++) {
	            payload[i] ^= mask[i%4];
	        }
	
			std::cout << "payload size = " << payload_length << std::endl;
			std::cout << "payload: ";
			for (unsigned int i=0; i<payload_length; ++i) {
				std::cout << (int)payload[i] << ' ';
			}
			std::cout << std::endl;
			std::copy(payload, payload + payload_length, target);
	
	        // base64 decode the data
			len = payload_length;
	//        len = b64_pton((const char*)payload, target+target_offset, targsize);
	
	        // Restore the first character of the next frame
	        payload[payload_length] = save_char;
	//        if (len < 0) {
	//            Logger(Level::WEBSOCKET) << "Base64 decode error code %d", len);
	//            return len;
	//        }
	        target_offset += len;
	
	        //printf("    len %d, raw %s\n", len, frame);
	    }
	
	    if (framecount > 1) {
	        snprintf(cntstr, 3, "%d", framecount);
	    }
	    
	    *left = remaining;
	    return target_offset;
	}
	
	
	
	int parse_handshake(ws_ctx_t *ws_ctx, char *handshake) {
	    char *start, *end;
	    headers_t *headers = ws_ctx->headers;
	
	    headers->key1[0] = '\0';
	    headers->key2[0] = '\0';
	    headers->key3[0] = '\0';
	    
	    if ((strlen(handshake) < 92) || (bcmp(handshake, "GET ", 4) != 0)) {
	        return 0;
	    }
	    start = handshake+4;
	    end = strstr(start, " HTTP/1.1");
	    if (!end) { return 0; }
	    strncpy(headers->path, start, end-start);
	    headers->path[end-start] = '\0';
	
	    start = strstr(handshake, "\r\nHost: ");
	    if (!start) { return 0; }
	    start += 8;
	    end = strstr(start, "\r\n");
	    strncpy(headers->host, start, end-start);
	    headers->host[end-start] = '\0';
	
	    headers->origin[0] = '\0';
	    start = strstr(handshake, "\r\nOrigin: ");
	    if (start) {
	        start += 10;
	    } else {
	        start = strstr(handshake, "\r\nSec-WebSocket-Origin: ");
	        if (!start) { return 0; }
	        start += 24;
	    }
	    end = strstr(start, "\r\n");
	    strncpy(headers->origin, start, end-start);
	    headers->origin[end-start] = '\0';
	   
	    start = strstr(handshake, "\r\nSec-WebSocket-Version: ");
	    if (start) {
	        // HyBi/RFC 6455
	        start += 25;
	        end = strstr(start, "\r\n");
	        strncpy(headers->version, start, end-start);
	        headers->version[end-start] = '\0';
	        ws_ctx->hybi = strtol(headers->version, NULL, 10);
	
	        start = strstr(handshake, "\r\nSec-WebSocket-Key: ");
	        if (!start) { return 0; }
	        start += 21;
	        end = strstr(start, "\r\n");
	        strncpy(headers->key1, start, end-start);
	        headers->key1[end-start] = '\0';
	   
	        start = strstr(handshake, "\r\nConnection: ");
	        if (!start) { return 0; }
	        start += 14;
	        end = strstr(start, "\r\n");
	        strncpy(headers->connection, start, end-start);
	        headers->connection[end-start] = '\0';
	   
	        start = strstr(handshake, "\r\nSec-WebSocket-Protocol: ");
	        if (!start) { return 0; }
	        start += 26;
	        end = strstr(start, "\r\n");
	        strncpy(headers->protocols, start, end-start);
	        headers->protocols[end-start] = '\0';
	    }
	
	    return 1;
	}
	
	static void gen_sha1(headers_t *headers, char *target) {
	
		static const size_t HYBI10_ACCEPTHDRLEN = 29;
		static const char HYBI_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
	    ::SHA_CTX c;
	    unsigned char hash[SHA_DIGEST_LENGTH];
	
	    ::SHA1_Init(&c);
	    ::SHA1_Update(&c, headers->key1, strlen(headers->key1));
	    ::SHA1_Update(&c, HYBI_GUID, 36);
	    ::SHA1_Final(hash, &c);
	
	    b64_ntop(hash, sizeof hash, target, HYBI10_ACCEPTHDRLEN);
	    //assert(r == HYBI10_ACCEPTHDRLEN - 1);
	}


	struct Handshake {

		ws_ctx_t *do_handshake(int sock) {
		    char handshake[4096], response[4096], sha1[29], trailer[17];
		    std::string scheme, pre;
		    headers_t *headers;
		    int len, i, offset;
		    ws_ctx_t * ws_ctx;
		
		    // Peek, but don't read the data
		    len = recv(sock, handshake, 1024, MSG_PEEK);
		    handshake[len] = 0;
		    if (len == 0) {
		        Logger(Level::WEBSOCKET) << "ignoring empty handshake" << std::endl;
		        return NULL;
		    } else if (bcmp(handshake, "<policy-file-request/>", 22) == 0) {
		        len = recv(sock, handshake, 1024, 0);
		        handshake[len] = 0;
		        Logger(Level::WEBSOCKET) << "sending flash policy response" << std::endl;
				// TODO: this line sends NULL character
		        send(sock, POLICY_RESPONSE, sizeof(POLICY_RESPONSE), 0);
		        return NULL;
		    } else if ((bcmp(handshake, "\x16", 1) == 0) ||
		               (bcmp(handshake, "\x80", 1) == 0)) {
		        // SSL
		        if (!settings.cert) {
		            Logger(Level::WEBSOCKET) << "SSL connection but no cert specified" << std::endl;
		            return NULL;
		        } else if (access(settings.cert, R_OK) != 0) {
		            Logger(Level::WEBSOCKET) << "SSL connection but '"
						<< settings.cert << "' not found" << std::endl;
		            return NULL;
		        }
		        ws_ctx = alloc_ws_ctx();
		        ws_socket_ssl(ws_ctx, sock, settings.cert, settings.key);
		        if (! ws_ctx) { return NULL; }
		        scheme = "wss";
		        Logger(Level::WEBSOCKET) << "using SSL socket" << std::endl;
		    } else if (settings.ssl_only) {
		        Logger(Level::WEBSOCKET) << "non-SSL connection disallowed" << std::endl;
		        return NULL;
		    } else {
		        ws_ctx = alloc_ws_ctx();
		        ws_socket(ws_ctx, sock);
		        if (! ws_ctx) { return NULL; }
		        scheme = "ws";
		        Logger(Level::WEBSOCKET) << "using plain (not SSL) socket" << std::endl;
		    }
		    offset = 0;
		    for (i = 0; i < 10; i++) {
		        len = ws_recv(ws_ctx, handshake+offset, 4096);
		        if (len == 0) {
		            Logger(Level::WEBSOCKET) << "Client closed during handshake" << std::endl;
		            return NULL;
		        }
		        offset += len;
		        handshake[offset] = 0;
		        if (strstr(handshake, "\r\n\r\n")) {
		            break;
		        }
		        usleep(10);
		    }
		
		    Logger(Level::WEBSOCKET) << "handshake: " <<  handshake << std::endl;
		    if (!parse_handshake(ws_ctx, handshake)) {
		        Logger(Level::WEBSOCKET) << "Invalid WS request" << std::endl;
		        return NULL;
		    }
		
			ws_ctx->subproto = BINARY;
		
		    headers = ws_ctx->headers;
		    if (ws_ctx->hybi > 0) {
		        Logger(Level::WEBSOCKET) << "using protocol HyBi/IETF 6455 " << ws_ctx->hybi << std::endl;
		        gen_sha1(headers, sha1);
		        sprintf(response, SERVER_HANDSHAKE_HYBI, sha1, SUBPROTOCOL_NAMES[ws_ctx->subproto]);
		    }
		    
		    //Logger(Level::WEBSOCKET) << "response: %s\n", response);
		    ws_send(ws_ctx, response, strlen(response));
		
		    return ws_ctx;
		}

		void clear() {
			handshake.clear();
		}

	private:
		std::stringstream handshake;
	};

	struct Web_socket: public Socket {

		enum State {
			HANDSHAKE,
			NORMAL
		};

		Web_socket(): _context(nullptr), _handshaker(nullptr) {}
		explicit Web_socket(const Socket& rhs): Socket(rhs), _context(nullptr), _handshaker(nullptr) {}

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
				if (!_handshaker) {
					_handshaker = new Handshake;
				}
				_context = _handshaker->do_handshake(_socket);
				if (_context) {
					Logger(Level::WEBSOCKET) << "Handshake completed" << std::endl;
					delete _handshaker;
				}
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
//            	ssize_t bytes = ws_recv(_context, _context->tin_buf + tin_end, BUFSIZE-1);
        		ssize_t bytes = ::read(_socket, _context->tin_buf + tin_end, BUFSIZE-1);
				int len = 0;
    			unsigned int left = 0, opcode = 0;
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
            	if (_context->hybi) {
            	    len = decode_hybi((unsigned char*)_context->tin_buf + tin_start,
            	                      tin_end-tin_start,
            	                      (unsigned char*)_context->tout_buf + tout_end, BUFSIZE-1,
            	                      &opcode, &left);
            	}
            	if (opcode == 8 || len < 0) {
					Logger(Level::WEBSOCKET) << "Close frame" << std::endl;
					return 0;
				}
            	if (left) {
            	    tin_start = tin_end - left;
            	    //printf("partial frame from client");
					Logger(Level::WEBSOCKET) << "Partial frame" << std::endl;
            	} else {
            	    tin_start = 0;
            	    tin_end = 0;
					Logger(Level::WEBSOCKET) << "End of frame" << std::endl;
            	}
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
			uint32_t ret = 0;
			if (_context) {
				unsigned int encoded_size;
    	        if (_context->hybi) {
    	            encoded_size = encode_hybi((unsigned char*)buf, size,
						_context->cout_buf + cout_end, BUFSIZE, Opcode::BINARY_FRAME);
    	        }
				cout_end += encoded_size;
				uint32_t bytes = ws_send(_context, _context->cout_buf + cout_start, cout_end - cout_start);
				cout_start += bytes;
				if (cout_start == cout_end) {
					ret = size;
					cout_start = 0;
					cout_end = 0;
				}
			}
			return ret;
		}

		friend std::ostream& operator<<(std::ostream& out, const Web_socket& rhs) {
			return out << rhs._context->sockfd;
		}

	private:
		ws_ctx_t* _context;
		Handshake* _handshaker;
    	unsigned int tin_start = 0;
		unsigned int tin_end = 0;
    	unsigned int tout_start = 0;
		unsigned int tout_end = 0;
    	unsigned int cout_start = 0;
		unsigned int cout_end = 0;
	};

}
