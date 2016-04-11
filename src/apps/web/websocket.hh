#ifndef APPS_WEB_WEBSOCKET_HH
#define APPS_WEB_WEBSOCKET_HH

#include <random>
#include <unordered_map>

#include <sys/io/socket.hh>
#include "lbuffer.hh"
#include "encoding.hh"

namespace factory {

	namespace components {

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

		typedef stdx::log<Web_socket> this_log;

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
					this_log() << "recv buffer"
						<< "(" << recv_buffer.size() << ") "
						<< std::setw(40) << recv_buffer << std::endl;
//					this_log() << "mid buffer "
//						<< std::setw(40) << mid_buffer << std::endl;
					recv_buffer.ignore(len);
				}
				if (opcode == Opcode::conn_close) {
					this_log() << "Close frame" << std::endl;
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
			send_buffer.flush<sys::socket&>(*this);
			this_log() << "send buffer"
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
						this_log() << "parsing headers" << std::endl;
					} else {
						state = State::PARSING_ERROR;
						this_log()
							<< "bad HTTP status in web socket hand shake" << std::endl;
					}
				}
			} else {
				state = State::PARSING_ERROR;
				this_log()
					<< "bad method in web socket hand shake" << std::endl;
			}
		}

		template<class It>
		void parse_http_method(It first, It last) {
			static const char GET[] = "GET";
			if (std::search(first, last, GET, GET + sizeof(GET)-1) != last) {
				state = State::PARSING_HEADERS;
				this_log() << "parsing headers" << std::endl;
			} else {
				state = State::PARSING_ERROR;
				this_log()
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
					this_log()
						<< "too many headers in HTTP request" << std::endl;
				} else if (_http_headers.contain(key.c_str())) {
					state = State::PARSING_ERROR;
					this_log()
						<< "duplicate HTTP header: '" << key << '\'' << std::endl;
				} else {
					_http_headers[key] = value;
					this_log()
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
					this_log() << "parsing error" << std::endl;
				} else {
					state = State::PARSING_SUCCESS;
					this_log() << "parsing success" << std::endl;
				}
			}
		}

		void reply_success() {
			this_log() << "replying success" << std::endl;
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
			this_log() << "writing handshake " << request.str() << std::endl;
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


//	template<class T>
//	struct basic_websocketbuf: public basic_fdbuf<T> {
//
//		using typename std::basic_streambuf<T>::int_type;
//		using typename std::basic_streambuf<T>::traits_type;
//		using typename std::basic_streambuf<T>::char_type;
//		using typename basic_fdbuf<T>::sys::fd_type;
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
//					this_log() << "recv header " << this->_frame << std::endl;
//					this->_nread = 0;
//					this->_orig->gbump(hdrsz);
//					this->sets(State::READING_PAYLOAD);
//					if (_frame.opcode() == opcode::conn_close) {
//						this_log() << "Close frame" << std::endl;
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
////				this->_frame.opcode(opcode::binary_frame);
////				this->_frame.fin(1);
////				this->_frame.payload_size(input_size);
////				this->_frame.mask(components::rng());
////				this->_frame.encode(result);
////				this->_frame.copy_payload(first, last, result);
////				this_log() << "send header " << this->frame << std::endl;
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
//						this_log() << "parsing headers" << std::endl;
//					} else {
//						this->sets(State::PARSING_ERROR);
//						this_log()
//							<< "bad HTTP status in web socket hand shake" << std::endl;
//					}
//				}
//			} else {
//				this->sets(State::PARSING_ERROR);
//				this_log()
//					<< "bad method in web socket hand shake" << std::endl;
//			}
//		}
//
//		template<class It>
//		void parse_http_method(It first, It last) {
//			static const char GET[] = "GET";
//			if (std::search(first, last, GET, GET + sizeof(GET)-1) != last) {
//				this->sets(State::PARSING_HEADERS);
//				this_log() << "parsing headers" << std::endl;
//			} else {
//				this->sets(State::PARSING_ERROR);
//				this_log()
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
//					this_log()
//						<< "too many headers in HTTP request" << std::endl;
//				} else if (_http_headers.contain(key.c_str())) {
//					this->sets(State::PARSING_ERROR);
//					this_log()
//						<< "duplicate HTTP header: '" << key << '\'' << std::endl;
//				} else {
//					this->_http_headers[key] = value;
//					this_log()
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
//					this_log() << "parsing error" << std::endl;
//				} else {
//					this->sets(State::PARSING_SUCCESS);
//					this_log() << "parsing success" << std::endl;
//				}
//			}
//		}
//
//		void reply_success() {
//			this_log() << "replying success" << std::endl;
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
//			this_log() << "writing handshake " << request.str() << std::endl;
//		}
//
//		void sets(State rhs) {
//			this->_state = rhs;
//			this_log() << "state=" << this->_state << std::endl;
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


	}

}

#endif // APPS_WEB_WEBSOCKET_HH
