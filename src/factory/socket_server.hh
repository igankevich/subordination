namespace factory {

	namespace components {

		template<class Kernel, class Stream, class Type>
		struct Kernel_packet {

			enum State {
				READING_SIZE,
				READING_PACKET,
				COMPLETE
			};

//			typedef decltype(out.size()) Size;
			typedef typename Stream::Size Size;

			void write(Stream& out, Kernel* kernel) {

				const Type* type = kernel->type();
				if (type == nullptr) {
					std::stringstream msg;
					msg << "Can not find type for kernel " << kernel->id();
					throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
				}

				auto old_pos = out.write_pos();
				Size old_size = out.size();
				out << Size(0);
				out << type->id();
				kernel->write(out);
				auto new_pos = out.write_pos();
				auto new_size = out.size();
				auto packet_size = new_size - old_size;
				Logger(Level::COMPONENT) << "Packet size = " << packet_size - sizeof(packet_size) << std::endl;
				out.write_pos(old_pos);
				out << packet_size;
				out.write_pos(new_pos);
			}

			bool read(Stream& in, Endpoint from) {
				if (_state == READING_SIZE && sizeof(packet_size) <= in.size()) {
					in >> packet_size;
					_state = READING_PACKET;
				}
				Logger(Level::COMPONENT)<< "Bytes received = "
					<< in.size()
					<< '/'
					<< packet_size - sizeof(packet_size)
					<< std::endl;
				if (_state == READING_PACKET && packet_size - sizeof(packet_size) <= in.size()) {
					Type::types().read_and_send_object(in, from);
					_state = COMPLETE;
				}
				return finished();
			}

			bool finished() const { return _state == COMPLETE; }

			void reset_reading_state() {
				_state = READING_SIZE;
				packet_size = 0;
			}

		private:
			State _state = READING_SIZE;
			Size packet_size = 0;
		};

		template<class Server, class Remote_server, class Kernel, class Type>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Type>, Server> {
			
			typedef Socket_server<Server, Remote_server, Kernel, Type> This;
			typedef typename Event_poller<Remote_server>::E Event;

			Socket_server():
				_poller(),
				_listener_socket(),
				_upstream(),
				_cpu(0),
				_thread(),
				_mutex()
			{}

			~Socket_server() {
//				std::for_each(_upstream.begin(), _upstream.end(),
//					[] (std::pair<Endpoint,Remote_server*> pair)
//				{
//					delete pair.second;
//				});
			}

			void process_event() {
				std::unique_lock<std::mutex> lock(_mutex);
				for (auto pair : _upstream) {
					process_event(pair.second, Event(EPOLLOUT, nullptr));
				}
			}
	
			void serve() {
//				process_event();
				_poller.run([this] (Event event) {
					Logger(Level::SERVER)
						<< "Event " << event.user_data()->endpoint()
						<< ' ' << event << std::endl;
					if (event.fd() == (int)_listener_socket) {
						std::pair<Socket,Endpoint> pair = _listener_socket.accept();
						Socket socket = pair.first;
						Endpoint endpoint = pair.second;
						std::unique_lock<std::mutex> lock(_mutex);
						Remote_server* handler = new Remote_server(socket, endpoint);
						_upstream[endpoint] = handler;
						lock.unlock();
						_poller.register_socket(Event(DEFAULT_EVENTS, handler));
					} else {
						if (event.is_error() || !event.user_data()->valid()) {
//							event.user_data()->valid();
							Logger(Level::SERVER) << "Invalid socket" << std::endl;
							remove(event.user_data());
							event.user_data()->recover_kernels();
							event.delete_user_data();
						} else {
							process_event(event.user_data(), event);
							Logger(Level::SERVER) << "Processed event" << std::endl;
							if (event.is_closing()) {
								remove(event.user_data());
								event.user_data()->recover_kernels();
								event.delete_user_data();
							}
						}
					}
					debug();
				});
			}

			void process_event(Remote_server* server, Event event) {
				server->handle_event(event, [this, &event] (bool overflow) {
//					std::unique_lock<std::mutex> lock(_mutex);
					if (overflow) {
						_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, event.user_data()));
					} else {
						_poller.modify_socket(Event(DEFAULT_EVENTS, event.user_data()));
					}
				});
			}

			void send(Kernel* kernel, Endpoint endpoint) {
				Logger(Level::SERVER) << "Socket_server::send(" << endpoint << ")" << std::endl;
				std::unique_lock<std::mutex> lock(_mutex);
				auto result = _upstream.find(endpoint);
				if (result == _upstream.end()) {
					Remote_server* handler = add_endpoint(endpoint, DEFAULT_EVENTS);
					handler->send(kernel);
					_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, handler));
				} else {
					result->second->send(kernel);
					_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
				}
			}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				if (kernel->moves_upstream()) {
					Logger(Level::SERVER) << "Socket_server::send()" << std::endl;
//					Logger(Level::SERVER) << "Upstream size = " << _upstream.size() << std::endl;
					if (_upstream.size() > 0) {
						auto result = _upstream.begin();
						Logger(Level::SERVER) << "Socket = " << result->second->socket() << std::endl;
						result->second->send(kernel);
						_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
					}
				} else {
					auto result = _upstream.find(kernel->from());
					if (result == _upstream.end()) {
						std::stringstream msg;
						msg << "Can not find upstream server " << kernel->from();
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}
					result->second->send(kernel);
					_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, result->second));
				}
			}

			void add(Endpoint endpoint) {
				std::unique_lock<std::mutex> lock(_mutex);
				add_endpoint(endpoint);
			}

			void remove(Remote_server* handler) {
				std::unique_lock<std::mutex> lock(_mutex);
				_upstream.erase(handler->endpoint());
				Logger(Level::SERVER) << "Removing " << handler->endpoint() << std::endl;
//				_poller.erase(Event(DEFAULT_EVENTS, handler));
			}
	
//			void remove(Endpoint endpoint) {
//				std::unique_lock<std::mutex> lock(_mutex);
//				auto result = _upstream.find(endpoint);
//				if (result == _upstream.end()) {
//					std::stringstream msg;
//					msg << "Can not find endpoint to remove: " << endpoint;
//					throw Error(msg.str(), __FILE__, __LINE__, __func__);
//				}
//				Logger(Level::SERVER) << "Removing " << endpoint << std::endl;
//				_poller.erase(Event(DEFAULT_EVENTS, result->second));
//				_upstream.erase(endpoint);
//			}

			void socket(Endpoint endpoint) {
				_listener_socket.bind(endpoint);
				_poller.register_socket(Event(DEFAULT_EVENTS, new Remote_server(_listener_socket, endpoint)));
				_listener_socket.listen();
			}

			void start() {
				Logger(Level::SERVER) << "Socket_server::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}
	
			void stop_impl() {
				Logger(Level::SERVER) << "Socket_server::stop_impl()" << std::endl;
				_poller.stop();
			}

			void wait_impl() {
				Logger(Level::SERVER) << "Socket_server::wait_impl()" << std::endl;
				if (_thread.joinable()) {
					_thread.join();
				}
				Logger(Level::SERVER) << "Socket_server::wait_impl() end" << std::endl;
			}

			void affinity(int cpu) { _cpu = cpu; }

			friend std::ostream& operator<<(std::ostream& out, const This* rhs) {
				return operator<<(out, *rhs);
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "sserver " << rhs._cpu;
			}

			void debug() {
				std::unique_lock<std::mutex> lock(_mutex);
				Logger log(Level::SERVER);
				log << _listener_socket.name();
				log << ": Upstream: ";
				for (auto p : _upstream) {
					log << *p.second << ',';
				}
				log << std::endl;
			}
		
		private:

			Remote_server* add_endpoint(Endpoint endpoint, int events = DEFAULT_EVENTS) {
				Socket socket;
				Remote_server* handler = new Remote_server(socket, endpoint);
				_upstream[endpoint] = handler;
				_poller.register_socket(Event(events, handler));
				socket.connect(endpoint);
//				Logger(Level::SERVER) << "Upstream size = " << _upstream.size() << std::endl;
				return handler;
			}

			void add_server(Remote_server* srv) {
				_upstream[srv->endpoint()] = srv;
				_poller.register_socket(Event(DEFAULT_EVENTS, srv));
			}
			
			Event_poller<Remote_server> _poller;
			Server_socket _listener_socket;
			std::map<Endpoint, Remote_server*> _upstream;

			// multi-threading
			int _cpu;
			std::thread _thread;
			std::mutex _mutex;

			static const int DEFAULT_EVENTS = EPOLLRDHUP | EPOLLIN;
		};

//	template<unsigned int N = 128>
//	struct Web_socket_handler: public Request_handler {
//
//		typedef factory::Kernel Kernel;
//
//		explicit Web_socket_handler(const Event& e, Socket_service<Web_socket_handler<N>>* service):
//			Request_handler(e),
//			_socket(e.data.fd),
//			_context(nullptr),
//			_stream(),
//			_service(service)
//		{
//			_socket.flags(O_NONBLOCK);
//		}
//
//		~Web_socket_handler() {
//			if (_context != nullptr) {
//				free_ws_ctx(_context);
//			}
//		}
//
//		void react(factory::Kernel* k) {
//
//			Request_handler* h = reinterpret_cast<Request_handler*>(k);
//
//			if (_context == nullptr) {
//				_context = ::do_handshake(_socket);
////				Server<Kernel, Strategy_remote>* srv = new Web_socket_server<Kernel, Resource_aware<Round_robin>>(_context);
////				local_repository()->put(_service, srv);
//			} else {
//				std::clog << "WebSocket message" << std::endl;
//				std::clog << "Reading stream" << std::endl;
//				unsigned int opcode;
//				unsigned int left;
//				ssize_t len;
//	            ssize_t bytes = ws_recv(_context, _context->tin_buf + tin_end, BUFSIZE-1);
//	            if (bytes <= 0) {
//	                handler_emsg("client closed connection\n");
//	            }
//	            tin_end += bytes;
//	            /*
//	            printf("before decode: ");
//	            for (i=0; i< bytes; i++) {
//	                printf("%u,", (unsigned char) *(_context->tin_buf+i));
//	            }
//	            printf("\n");
//	            */
//	            if (_context->hybi) {
//	                len = decode_hybi((unsigned char*)(_context->tin_buf + tin_start),
//	                                  tin_end-tin_start,
//	                                  (unsigned char*)(_context->tout_buf), BUFSIZE-1,
//	                                  &opcode, &left);
//	            } else {
//	                len = decode_hixie(_context->tin_buf + tin_start,
//	                                   tin_end-tin_start,
//	                                   (unsigned char*)(_context->tout_buf), BUFSIZE-1,
//	                                   &opcode, &left);
//	            }
//	
//	            if (opcode == 8) {
//	                handler_emsg("client sent orderly close frame\n");
//	            }
//	
//	            printf("decoded: ");
//	            for (int i=0; i<len; i++) {
//					std::cout << _context->tout_buf[i];
////	                printf("%u,", (unsigned char) *(_context->tout_buf+i));
//	            }
//	            printf("\n");
//	            if (len < 0) {
//	                handler_emsg("decoding error\n");
//	            }
//	            if (left) {
//	                tin_start = tin_end - left;
//	                //printf("partial frame from client");
//	            } else {
//	                tin_start = 0;
//	                tin_end = 0;
//	            }
//	
//	            traffic("}");
//				std::clog << "Read end" << std::endl;
//			}
////			ssize_t bytes_read;
////			while ((bytes_read = ::read(_socket, _buffer, N)) > 0) {
////				std::clog << "Bytes read = " << bytes_read << std::endl;
////			}
//
//			if (h->is_close_event()) {
//				std::clog << "Close web socket connection" << std::endl;
//				commit(the_server());
//			}
//		}
//
//		void handle_event(const Event& e) {
//			downstream(the_server(), new Request_handler(e));
//		}
//
//		Socket socket() const { return Socket(_event.data.fd); }
//
//	private:
//		Socket _socket;
//		ws_ctx_t* _context;
//		unsigned int tin_end = 0;
//		unsigned int tin_start = 0;
//		Foreign_stream _stream;
//		Socket_service<Web_socket_handler<N>>* _service;
//	};

		template<class Kernel, template<class X> class Pool, class Type>
		struct Remote_Rserver {

			typedef Remote_Rserver<Kernel, Pool, Type> This;
			typedef Kernel_packet<Kernel, Foreign_stream, Type> Packet;
			typedef typename Foreign_stream::Pos Pos;

			Remote_Rserver(Socket socket, Endpoint endpoint):
				_socket(socket),
				_endpoint(endpoint),
				_istream(),
				_ostream(),
				_ipacket(),
				_mutex(),
				_pool(),
				_buffer()
			{
			}

			~Remote_Rserver() {
//				recover_kernels();
			}

			void recover_kernels() {

				clear_kernel_buffer(_ostream.global_read_pos());

				Logger(Level::HANDLER)
					<< "Kernels left: "
					<< _pool.size()
					<< '+'
					<< _buffer.size()
					<< std::endl;
				
				// return kernels to their parents with an error
				while (!_pool.empty()) {
					recover_kernel(_pool.front());
					_pool.pop();
				}
				
				// recover kernels written to output buffer
				while (!_buffer.empty()) {
					recover_kernel(_buffer.front().second);
					_buffer.pop();
				}
			}

			static void recover_kernel(Kernel* k) {
				k->result(Result::ENDPOINT_NOT_CONNECTED);
				k->principal(k->parent());
				factory_send(k);
			}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				Logger(Level::HANDLER) << "Remote_Rserver::send()" << std::endl;
				_pool.push(kernel);
			}

			bool valid() const {
//				TODO: It is probably slow to check error on every event.
				return _socket.error() == 0;
			}

			template<class F>
			void handle_event(Event<This> event, F on_overflow) {
				bool overflow = false;
				if (event.is_reading()) {
					_istream.fill<Socket>(_socket);
					bool state_is_ok = true;
					while (state_is_ok && !_istream.empty()) {
						Logger(Level::HANDLER) << "Recv " << _istream << std::endl;
						try {
							state_is_ok = _ipacket.read(_istream, _endpoint);
						} catch (No_principal_found<Kernel>& err) {
							Logger(Level::HANDLER) << "No principal found for "
								<< int(err.kernel()->result()) << std::endl;
							Kernel* k = err.kernel();
							k->principal(k->parent());
							_pool.push(k);
							overflow = true;
						}
						if (state_is_ok) {
							_ipacket.reset_reading_state();
						}
					}
				}
				if (event.is_writing()) {
//					try {
						std::unique_lock<std::mutex> lock(_mutex);
						while (!_pool.empty()) {
							Kernel* kernel = _pool.front();
							_pool.pop();
							Packet packet;
							packet.write(_ostream, kernel);
							if (kernel->result() == Result::NO_PRINCIPAL_FOUND) {
								Logger(Level::HANDLER) << "poll send error " << _ostream << std::endl;
							}
							_buffer.push(std::make_pair(_ostream.global_write_pos(), kernel));
						}
						Logger(Level::HANDLER) << "Send " << _ostream << std::endl;
						Logger(Level::HANDLER) << "Flushing" << std::endl;
						_ostream.flush<Socket>(_socket);
						if (_ostream.empty()) {
							Logger(Level::HANDLER) << "Flushed." << std::endl;
							clear_kernel_buffer();
							_ostream.reset();
						} else {
							clear_kernel_buffer(_ostream.global_read_pos());
							overflow = true;
						}
						Logger(Level::HANDLER) << "Overflowing" << std::endl;
//					} catch (Connection_error& err) {
//						Logger(Level::HANDLER) << Error_message(err, __FILE__, __LINE__, __func__);
//					} catch (Error& err) {
//						Logger(Level::HANDLER) << Error_message(err, __FILE__, __LINE__, __func__);
//					} catch (std::exception& err) {
//						Logger(Level::HANDLER) << String_message(err, __FILE__, __LINE__, __func__);
//					} catch (...) {
//						Logger(Level::HANDLER) << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
//					}
				}
				on_overflow(overflow);
			}

			int fd() const { return _socket; }
			Socket socket() const { return _socket; }
			Endpoint endpoint() const { return _endpoint; }

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
//				std::unique_lock<std::mutex> lock(rhs._mutex);
				return out << rhs.endpoint() << '('
					<< (rhs.valid() ? ' ' : '!')
					<< rhs._pool.size() << ','
					<< rhs._buffer.size() << ','
					<< rhs._istream.size() << ','
					<< rhs._ostream.size() << ')';
			}

		private:

			void clear_kernel_buffer() {
				while (!_buffer.empty()) {
					delete _buffer.front().second;
					_buffer.pop();
				}
			}

			void clear_kernel_buffer(Pos global_read_pos) {
				while (!_buffer.empty() && _buffer.front().first <= global_read_pos) {
					delete _buffer.front().second;
					_buffer.pop();
				}
			}

			Server_socket _socket;
			Endpoint _endpoint;
			Foreign_stream _istream;
			Foreign_stream _ostream;
			Packet _ipacket;
			std::mutex _mutex;
			Pool<Kernel*> _pool;
			std::queue<std::pair<Pos,Kernel*>> _buffer;
		};

	}

}

