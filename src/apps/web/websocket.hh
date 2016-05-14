#ifndef APPS_WEB_WEBSOCKET_HH
#define APPS_WEB_WEBSOCKET_HH

#include <random>
#include <unordered_map>

#include <sys/socket.hh>
#include "lbuffer.hh"
#include "encoding.hh"

namespace factory {

		/*
		 * WebSocket lib with support for "wss://" encryption.
		 * Copyright 2010 Joel Martin
		 * Licensed under LGPL version 3 (see docs/LICENSE.LGPL-3)
		 *
		 * You can make a cert/key with openssl using:
		 * openssl req -new -x509 -days 365 -nodes -out self.pem -keyout self.pem
		 * as taken from http://docs.python.org/dev/library/ssl.html#certificates
		 */


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

	struct Web_socket: public sys::socket {

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
			mid_buffer(BUFFER_SIZE),
			_rng()
			{}

//		explicit Web_socket(sys::fd_type fd):
//			sys::socket(fd),
//			_http_headers(),
//			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
//			send_buffer(BUFFER_SIZE),
//			recv_buffer(BUFFER_SIZE),
//			mid_buffer(BUFFER_SIZE)
//			{}

		// TODO: move all fields
		explicit Web_socket(Web_socket&& rhs):
			sys::socket(std::move(rhs)),
			_http_headers(),
			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE),
			_rng()
			{}

		explicit Web_socket(sys::socket&& rhs):
			sys::socket(std::move(rhs)),
			_http_headers(),
			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE),
			_rng()
			{}

		explicit Web_socket(sys::fd_type rhs):
			sys::socket(rhs),
			_http_headers(),
			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
			send_buffer(BUFFER_SIZE),
			recv_buffer(BUFFER_SIZE),
			mid_buffer(BUFFER_SIZE),
			_rng()
			{}

		explicit
		Web_socket(const sys::endpoint& bind_addr, const sys::endpoint& conn_addr):
		sys::socket(bind_addr, conn_addr),
		_http_headers(),
		key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
		send_buffer(BUFFER_SIZE),
		recv_buffer(BUFFER_SIZE),
		mid_buffer(BUFFER_SIZE),
		_rng()
		{}

//		explicit Web_socket(const sys::socket& rhs):
//			sys::socket(rhs),
//			_http_headers(),
//			key(WEBSOCKET_KEY_BASE64_LENGTH, 0),
//			send_buffer(BUFFER_SIZE),
//			recv_buffer(BUFFER_SIZE),
//			mid_buffer(BUFFER_SIZE)
//			{}

		virtual ~Web_socket() { this->close(); }

		Web_socket& operator=(Web_socket&& rhs) {
			sys::socket::operator=(std::move(rhs));
			return *this;
		}

//		Web_socket& operator=(const sys::socket& rhs) {
//			sys::socket::operator=(rhs);
//			return *this;
//		}

		uint32_t write(const void* buf2, size_t size) {
			const char* buf = static_cast<const char*>(buf2);
			uint32_t ret = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				if (state == State::INITIAL_STATE) {
					state = State::STARTING_HANDSHAKE;
					role = Role::CLIENT;
				}
				handshake();
			} else {
				websocket_encode(buf, buf + size,
					std::back_inserter(send_buffer), _rng);
				this->flush();
				ret = size;
			}
			return ret;
		}

		uint32_t read(void* buf2, size_t size) {
			char* buf = static_cast<char*>(buf2);
			uint32_t bytes_read = 0;
			if (state != State::HANDSHAKE_SUCCESS) {
				if (state == State::INITIAL_STATE) {
					state = State::PARSING_HTTP_METHOD;
					role = Role::SERVER;
				}
				handshake();
			} else {
				Opcode opcode = Opcode::cont_frame;
				if (mid_buffer.empty()) {
					this->fill();
					size_t len = websocket_decode(recv_buffer.read_begin(),
						recv_buffer.read_end(), std::back_inserter(mid_buffer),
						&opcode);
					#ifndef NDEBUG
					stdx::debug_message("wbs")
						<< "recv buffer"
						<< "(" << recv_buffer.size() << ") "
						<< std::setw(40) << recv_buffer;
					#endif
					recv_buffer.ignore(len);
				}
				if (opcode == Opcode::conn_close) {
					#ifndef NDEBUG
					stdx::debug_message("wbs", "Close frame");
					#endif
					this->close();
				} else {
					bytes_read = mid_buffer.read(buf, size);
				}
			}
			return bytes_read;
		}

		bool empty() const { return send_buffer.empty(); }

		bool flush() {
			#ifndef NDEBUG
			size_t old_size = send_buffer.size();
			#endif
			send_buffer.flush<sys::socket&>(*this);
			#ifndef NDEBUG
			stdx::debug_message("wbs")
				<< "send buffer"
				<< '(' << old_size - send_buffer.size() << ')';
			#endif
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
						#ifndef NDEBUG
						stdx::debug_message("wbs", "parsing headers");
						#endif
					} else {
						state = State::PARSING_ERROR;
						#ifndef NDEBUG
						stdx::debug_message("wbs", "bad HTTP status in web socket hand shake");
						#endif
					}
				}
			} else {
				state = State::PARSING_ERROR;
				#ifndef NDEBUG
				stdx::debug_message("wbs", "bad method in web socket hand shake");
				#endif
			}
		}

		template<class It>
		void parse_http_method(It first, It last) {
			static const char GET[] = "GET";
			if (std::search(first, last, GET, GET + sizeof(GET)-1) != last) {
				state = State::PARSING_HEADERS;
				#ifndef NDEBUG
				stdx::debug_message("wbs", "parsing headers");
				#endif
			} else {
				state = State::PARSING_ERROR;
				#ifndef NDEBUG
				stdx::debug_message("wbs", "bad method in web socket hand shake");
				#endif
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
					#ifndef NDEBUG
					stdx::debug_message("wbs", "too many headers in HTTP request");
					#endif
				} else if (_http_headers.contain(key.c_str())) {
					state = State::PARSING_ERROR;
					#ifndef NDEBUG
					stdx::debug_message("wbs") << "duplicate HTTP header: '" << key << '\'';
					#endif
				} else {
					_http_headers[key] = value;
					#ifndef NDEBUG
					stdx::debug_message("wbs") << "Header['" << key << "'] = '" << value << "'";
					#endif
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
					#ifndef NDEBUG
					stdx::debug_message("wbs", "parsing error");
					#endif
				} else {
					state = State::PARSING_SUCCESS;
					#ifndef NDEBUG
					stdx::debug_message("wbs", "parsing success");
					#endif
				}
			}
		}

		void reply_success() {
			#ifndef NDEBUG
			stdx::debug_message("wbs", "replying success");
			#endif
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
			websocket_key(this->key.begin(), _rng);
			std::stringstream request;
			request
				<< "GET / HTTP/1.1" << HTTP_FIELD_SEPARATOR
				// TODO: version
				<< "User-Agent: Factory/" << "0.2" << HTTP_FIELD_SEPARATOR
				<< "Connection: Upgrade" << HTTP_FIELD_SEPARATOR
				<< "Upgrade: websocket" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Protocol: binary" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Version: 13" << HTTP_FIELD_SEPARATOR
				<< "Sec-WebSocket-Key: " << key << HTTP_FIELD_SEPARATOR
				<< HTTP_FIELD_SEPARATOR;
			std::string buf = request.str();
			send_buffer.write(buf.data(), buf.size());
			state = State::WRITING_HANDSHAKE;
			#ifndef NDEBUG
			stdx::debug_message("wbs", "writing handshake") << request.str();
			#endif
		}

		void fill() { recv_buffer.fill<sys::socket&>(*this); }

		State state = State::INITIAL_STATE;
		Role role = Role::SERVER;
		HTTP_headers _http_headers;
		std::string key;

		LBuffer<char> send_buffer;
		LBuffer<char> recv_buffer;
		LBuffer<char> mid_buffer;

		std::random_device _rng;

		static const size_t BUFFER_SIZE = 1024*16;
		static const size_t MAX_HEADERS = 20;
		static const size_t ACCEPT_HEADER_LENGTH = 29;
		static const size_t WEBSOCKET_KEY_BASE64_LENGTH = 24;

		constexpr static const char* HTTP_FIELD_SEPARATOR = "\r\n";
		constexpr static const char* BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\n\r\n";
	};

}

#endif // APPS_WEB_WEBSOCKET_HH
