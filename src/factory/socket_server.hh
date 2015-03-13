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

		template<class Server, class Remote_server, class Kernel, class Type, template<class X> class Pool>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Type, Pool>, Server> {
			
			typedef Socket_server<Server, Remote_server, Kernel, Type, Pool> This;

			Socket_server():
				_poller(),
				_socket(),
				_upstream(),
				_servers(),
				_pool(),
				_cpu(0),
				_thread(),
				_mutex()
			{}

			void serve() {
				process_kernels();
				_poller.run([this] (Event event) {
					if (_poller.stopping()) {
						event.no_reading();
					}
					Logger(Level::SERVER)
						<< "Event " << event.fd() << ' ' << event << std::endl;
					if (event.fd() == _poller.notification_pipe()) {
						Logger(Level::SERVER) << "Notification " << event << std::endl;
						process_kernels();
					} else if (event.fd() == (int)_socket) {
						if (event.is_reading()) {
							auto pair = _socket.accept();
							Socket socket = pair.first;
							Endpoint addr = pair.second;
							Endpoint vaddr = virtual_addr(addr);
							auto res = _upstream.find(vaddr);
							if (res == _upstream.end()) {
								Remote_server* s = peer(socket, addr, vaddr, DEFAULT_EVENTS);
								Logger(Level::SERVER)
									<< server_addr() << ": "
									<< "connected peer " << s->vaddr() << std::endl;
							} else {
								Remote_server* s = res->second;
								Logger(Level::SERVER)
									<< "ports: "
									<< addr.port() << ' '
									<< s->bind_addr().port()
									<< std::endl;
								if (addr.port() < s->bind_addr().port()) {
									Logger log(Level::SERVER);
									log << server_addr() << ": "
										<< "not replacing peer " << *s
										<< std::endl;
									socket.no_reading();
									s->fill_from(socket);
									socket.close();
								} else {
									Logger log(Level::SERVER);
									log << server_addr() << ": "
										<< "replacing peer " << *s;
									_poller.ignore(s->fd());
									Remote_server* new_s = new Remote_server(std::move(*s));
									new_s->socket(socket);
									_servers.erase(s->fd());
									_servers[socket] = new_s;
									_upstream[vaddr] = new_s;
									_poller.add(Event(DEFAULT_EVENTS | POLLOUT, socket));
//									erase(s->bind_addr());
//									peer(socket, addr, vaddr, DEFAULT_EVENTS);
									log << " with " << *new_s << std::endl;
									debug("replacing upstream");
									delete s;
								}
							}
						}
					} else {
						auto res = _servers.find(event.fd());
						if (res == _servers.end()) {
							std::stringstream msg;
							msg << "Can not find server to process event: fd=" << event.fd();
							throw Error(msg.str(), __FILE__, __LINE__, __func__);
						}
						Remote_server* s = res->second;
						bool erasing;
						if (event.is_error() || !s->valid()) {
							Logger(Level::SERVER) << "Invalid socket" << std::endl;
							erasing = true;
						} else {
							process_event(s, event);
							erasing = event.is_closing();
						}
						if (erasing) {
							erase(event.fd());
						}
					}
//					if (_poller.stopping()) {
//						process_kernels();
//						flush_kernels();
//					}
					if (_poller.stopping() && !this->empty()) {
						debug("not stopping");
					}
					if (_poller.stopping() && this->empty()) {
						debug("stopping");
						_poller.stop();
					}
//					debug();
				});
//				if (_poller.stopped()) {
//					process_kernels();
//					flush_kernels();
//				}
			}

			bool empty() const {
				return std::all_of(_upstream.cbegin(), _upstream.cend(),
					[] (const std::pair<Endpoint,Remote_server*>& rhs)
				{
					return rhs.second->empty();
				});
			}

			// TODO: delete
			void send(Kernel* kernel, Endpoint endpoint) {
				Logger(Level::SERVER) << "Socket_server::send(" << endpoint << ")" << std::endl;
				kernel->to(endpoint);
				send(kernel);
			}

			void send(Kernel* kernel) {
//				if (this->stopped()) {
//					throw Error("Can not send kernel when the server is stopped.",
//						__FILE__, __LINE__, __func__);
//				}
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
				lock.unlock();
				_poller.notify();
			}

			void peer(Endpoint addr) {
//				if (!this->stopped()) {
//					throw Error("Can not add upstream server when socket server is running.",
//						__FILE__, __LINE__, __func__);
//				}
				peer(addr, DEFAULT_EVENTS);
			}

			void erase(Endpoint addr) {
				Logger(Level::SERVER) << "Removing " << addr << std::endl;
				auto res = _upstream.find(addr);
				if (res == _upstream.end()) {
					std::stringstream msg;
					msg << "Can not find server to erase: " << addr;
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				Remote_server* s = res->second;
				_upstream.erase(res);
				_servers.erase(s->socket());
				delete s;
			}

			void socket(Endpoint addr) {
				_socket.bind(addr);
				_socket.listen();
				_poller.add(Event(DEFAULT_EVENTS, _socket));
			}

			void start() {
				Logger(Level::SERVER) << "Socket_server::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}
	
			void stop_impl() {
				Logger(Level::SERVER) << "Socket_server::stop_impl()" << std::endl;
				_poller.notify_stopping();
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
		
		private:

			Endpoint virtual_addr(Endpoint addr) const {
				Endpoint vaddr = addr;
				vaddr.port(server_addr().port());
				return vaddr;
			}

			void erase(int fd) {
				auto r = _servers.find(fd);
				if (r == _servers.end()) {
					std::stringstream msg;
					msg << "Can not find server to erase: fd=" << fd;
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				Remote_server* s = r->second;
				Logger(Level::SERVER) << "Removing server " << *s << std::endl;
				s->recover_kernels();
				_upstream.erase(s->vaddr());
				_servers.erase(fd);
				delete s;
			}
	

			Endpoint server_addr() const { return _socket.name(); }

			void process_event(Remote_server* server, Event event) {
				server->handle_event(event, [this, &event] (bool overflow) {
					if (overflow) {
						_poller[event.fd()]->writing();
					} else {
						_poller[event.fd()]->events(DEFAULT_EVENTS);
					}
				});
			}

			void flush_kernels() {
				for (auto pair : _upstream) {
					Remote_server* server = pair.second;
					process_event(server, Event(POLLOUT, server->fd()));
				}
			}

			void process_kernels() {
				Logger(Level::SERVER) << "Socket_server::process_kernels()" << std::endl;
				bool empty = false;
				{
					std::unique_lock<std::mutex> lock(_mutex);
					empty = _pool.empty();
				}
				while (!empty) {

					std::unique_lock<std::mutex> lock(_mutex);
					Kernel* k = _pool.front();
					_pool.pop();
					empty = _pool.empty();
					lock.unlock();

					if (k->moves_upstream() && k->to() == Endpoint()) {
						if (k->broadcast()) {
							Logger(Level::SERVER)
								<< server_addr() << ' '
								<< "broadcast kernel" << std::endl;
							for (auto pair : _upstream) {
								Remote_server* s = pair.second;
								s->send(k);
								_poller[s->fd()]->writing();
							}
						} else {
							if (_upstream.empty()) {
								throw Error("No upstream servers found.", __FILE__, __LINE__, __func__);
							}
							// TODO: round robin
							auto result = _upstream.begin();
							result->second->send(k);
							_poller[result->second->fd()]->writing();
						}
					} else {
						// create endpoint if necessary, and send kernel
						if (k->to() == Endpoint()) {
							k->to(k->from());
						}
						auto result = _upstream.find(k->to());
						if (result == _upstream.end()) {
							Remote_server* handler = peer(k->to(), DEFAULT_EVENTS | POLLOUT);
							handler->send(k);
						} else {
							result->second->send(k);
							_poller[result->second->fd()]->writing();
						}
					}
				}
			}

			void debug(const char* msg = "Upstream") {
				Logger log(Level::SERVER);
				log << _socket.bind_addr() << ' ';
				log << msg << ' ';
				for (auto p : _upstream) {
					log << *p.second << ',';
				}
				log << std::endl;
			}

			Remote_server* peer(Endpoint addr, int events) {
				// bind to server address with ephemeral port
				Endpoint srv_addr = server_addr();
				srv_addr.port(0);
				Socket socket;
				socket.bind(srv_addr);
				socket.connect(addr);
				return peer(socket, socket.bind_addr(), addr, events);
			}

			Remote_server* peer(Socket socket, Endpoint addr, Endpoint vaddr, int events) {
				Remote_server* s = new Remote_server(socket, addr);
				s->vaddr(vaddr);
				_upstream[vaddr] = s;
				_servers[socket] = s;
				_poller.add(Event(events, socket));
				return s;
			}

			Poller _poller;
			Server_socket _socket;
			std::map<Endpoint, Remote_server*> _upstream;
			std::unordered_map<int, Remote_server*> _servers;
			Pool<Kernel*> _pool;

			// multi-threading
			int _cpu;
			std::thread _thread;
			std::mutex _mutex;

			static const int DEFAULT_EVENTS = POLLRDHUP | POLLIN;
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
				_vaddr(endpoint),
				_istream(),
				_ostream(),
				_ipacket(),
				_buffer() {}

			Remote_Rserver(Remote_Rserver&& rhs):
				_socket(std::move(rhs._socket)),
				_vaddr(rhs._vaddr),
				_istream(std::move(rhs._istream)),
				_ostream(std::move(rhs._ostream)),
				_ipacket(rhs._ipacket),
				_buffer(std::move(rhs._buffer)) {}

			void recover_kernels() {

				clear_kernel_buffer(_ostream.global_read_pos());

				Logger(Level::HANDLER)
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;
				
				// recover kernels written to output buffer
				while (!_buffer.empty()) {
					recover_kernel(_buffer.front().second);
					_buffer.pop();
				}
			}

			void send(Kernel* kernel) {
				Logger(Level::HANDLER) << "Remote_Rserver::send()" << std::endl;
				Packet packet;
				packet.write(_ostream, kernel);
				if (kernel->result() == Result::NO_PRINCIPAL_FOUND) {
					Logger(Level::HANDLER) << "poll send error " << _ostream << std::endl;
				}
				_buffer.push(std::make_pair(_ostream.global_write_pos(), kernel));
			}

			bool valid() const {
//				TODO: It is probably too slow to check error on every event.
				return _socket.error() == 0;
			}

			template<class F>
			void handle_event(Event event, F on_overflow) {
				bool overflow = false;
				if (event.is_reading()) {
					_istream.fill<Socket>(_socket);
					bool state_is_ok = true;
					while (state_is_ok && !_istream.empty()) {
						Logger(Level::HANDLER) << "Recv " << _istream << std::endl;
						try {
							state_is_ok = _ipacket.read(_istream, _vaddr);
						} catch (No_principal_found<Kernel>& err) {
							Logger(Level::HANDLER) << "No principal found for "
								<< int(err.kernel()->result()) << std::endl;
							Kernel* k = err.kernel();
							k->principal(k->parent());
							send(k);
							overflow = true;
						}
						if (state_is_ok) {
							_ipacket.reset_reading_state();
						}
					}
				}
				if (event.is_writing()) {
//					try {
						Logger(Level::HANDLER) << "Send " << _ostream << std::endl;
						_ostream.flush<Socket>(_socket);
						if (_ostream.empty()) {
							Logger(Level::HANDLER) << "Flushed." << std::endl;
							clear_kernel_buffer();
							_ostream.reset();
						} else {
							clear_kernel_buffer(_ostream.global_read_pos());
							overflow = true;
						}
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

			void fill_from(Socket s) {
				_istream.fill<Socket>(s);
			}

			int fd() const { return _socket; }
			Socket socket() const { return _socket; }
			void socket(Socket rhs) {
				_istream.fill<Socket>(_socket);
				_socket = rhs;
			}
			Endpoint bind_addr() const { return _socket.bind_addr(); }
			Endpoint vaddr() const { return _vaddr; }
			void vaddr(Endpoint rhs) { _vaddr = rhs; }

			bool empty() const { return _buffer.empty(); }

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << '('
//					<< rhs.bind_addr() << " <--> "
					<< rhs.vaddr() << ','
					<< rhs.fd() << ','
					<< (rhs.valid() ? ' ' : '!')
					<< rhs._buffer.size() << ','
					<< rhs._istream.size() << ','
					<< rhs._ostream.size() << ')';
			}

		private:

			static void recover_kernel(Kernel* k) {
				k->result(Result::ENDPOINT_NOT_CONNECTED);
				k->principal(k->parent());
				factory_send(k);
			}

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
			Endpoint _vaddr;
			Foreign_stream _istream;
			Foreign_stream _ostream;
			Packet _ipacket;
			std::queue<std::pair<Pos,Kernel*>> _buffer;
		};

	}

}

