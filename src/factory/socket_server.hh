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
			typedef typename Type::Callback Callback;

			Kernel_packet() {}

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
				auto packet_sz = new_size - old_size;
				Logger(Level::COMPONENT) << "Packet size = " << packet_sz - sizeof(packet_sz) << std::endl;
				out.write_pos(old_pos);
				out << packet_sz;
				out.write_pos(new_pos);
			}

			bool read(Stream& in, Callback callback) {
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
					Type::types().read_and_send_object(in, callback);
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
					} else if (event.fd() == _socket.fd()) {
						if (event.is_reading()) {
							auto pair = _socket.accept();
							Socket sock = pair.first;
							Endpoint addr = pair.second;
							Endpoint vaddr = virtual_addr(addr);
//							Endpoint vaddr = addr;
							auto res = _upstream.find(vaddr);
							if (res == _upstream.end()) {
								Remote_server* s = peer(sock, addr, vaddr, DEFAULT_EVENTS);
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
									// create temporary subordinate server
									// to read kernels until the socket
									// is closed from the other end
									Remote_server* new_s = new Remote_server(sock, addr);
									new_s->vaddr(vaddr);
									new_s->parent(s);
									_servers[sock] = new_s;
									_poller.add(Event(DEFAULT_EVENTS, sock));
//									sock.no_reading();
//									s->fill_from(sock);
//									sock.close();
									debug("not replacing upstream");
								} else {
									Logger log(Level::SERVER);
									log << server_addr() << ": "
										<< "replacing peer " << *s;
									_poller.ignore(s->fd());
									_servers.erase(s->fd());
									Remote_server* new_s = new Remote_server(std::move(*s));
									new_s->socket(sock);
									_servers[sock] = new_s;
									_upstream[vaddr] = new_s;
									Event ev(DEFAULT_EVENTS | POLLOUT, sock);
									ev.reading();
									ev.writing();
									_poller.add(ev);
//									erase(s->bind_addr());
//									peer(sock, addr, vaddr, DEFAULT_EVENTS);
									log << " with " << *new_s << std::endl;
									debug("replacing upstream");
									delete s;
								}
							}
						}
					} else {
						bool erasing = false;
						if (event.is_error()) {
							Logger(Level::SERVER) << "Invalid socket" << std::endl;
							erasing = true;
						} else {
							auto res = _servers.find(event.fd());
							if (res == _servers.end()) {
								debug("qqq");
								std::stringstream msg;
								msg << ::getpid() << ' ' << server_addr() << ' ';
								msg << " can not find server to process event: fd=" << event.fd();
								Logger(Level::SERVER) << msg.str() << std::endl;
								throw Error(msg.str(), __FILE__, __LINE__, __func__);
							}
							Remote_server* s = res->second;
							if (!s->valid()) {
								Logger(Level::SERVER) << "Invalid socket" << std::endl;
								erasing = true;
							} else {
								process_event(s, event);
								erasing = event.is_closing() || event.is_error();
							}
						}
						if (erasing) {
							erase(event.fd());
						}
					}
//					if (_poller.stopping()) {
//						process_kernels();
//						flush_kernels();
//					}
					if (_poller.stopping()) {
						++_stop_iterations;
						if (this->empty() || _stop_iterations == MAX_STOP_ITERATIONS) {
							debug("stopping");
							_poller.stop();
						} else {
							debug("not stopping");
						}
					}
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
//			void send(Kernel* kernel, Endpoint endpoint) {
//				Logger(Level::SERVER) << "Socket_server::send(" << endpoint << ")" << std::endl;
//				kernel->to(endpoint);
//				send(kernel);
//			}

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

			Endpoint server_addr() const { return _socket.bind_addr(); }
		
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
				// subordinate servers are not present in upstream
				if (!s->parent()) {
					_upstream.erase(s->vaddr());
				}
				_servers.erase(fd);
				delete s;
			}
	

			void process_event(Remote_server* server, Event event) {
				server->handle_event(event, [this, &event, server] (bool overflow) {
					if (overflow) {
						_poller[event.fd()]->writing();
						// Failed kernels are sent to parent,
						// so we need to fire write event.
						if (server->parent() != nullptr) {
							_poller[server->parent()->fd()]->writing();
						}
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
				bool pool_is_empty = false;
				{
					std::unique_lock<std::mutex> lock(_mutex);
					pool_is_empty = _pool.empty();
				}
				while (!pool_is_empty) {

					std::unique_lock<std::mutex> lock(_mutex);
					Kernel* k = _pool.front();
					_pool.pop();
					pool_is_empty = _pool.empty();
					lock.unlock();

					if (server_addr() && k->to() == server_addr()) {
						throw Error("Kernel is sent to local node.", __FILE__, __LINE__, __func__);
					}

					if (k->moves_everywhere()) {
						Logger(Level::SERVER)
							<< server_addr() << ' '
							<< "broadcast kernel" << std::endl;
						for (auto pair : _upstream) {
							Remote_server* s = pair.second;
							s->send(k);
							_poller[s->fd()]->writing();
						}
					} else if (k->moves_upstream() && k->to() == Endpoint()) {
						if (_upstream.empty()) {
							throw Error("No upstream servers found.", __FILE__, __LINE__, __func__);
						}
						// TODO: round robin
						auto result = _upstream.begin();
						result->second->send(k);
						_poller[result->second->fd()]->writing();
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

			void debug(const char* msg = "") {
				Logger log(Level::SERVER);
				log << _socket.bind_addr() << ' ';
				log << msg << " upstream ";
				for (auto p : _upstream) {
					log << *p.second << ',';
				}
				log << std::endl;
				log << _socket.bind_addr() << ' ';
				log << msg << " servers ";
				for (auto p : _servers) {
					log << p.first << " => ";
					log << *p.second << ',';
				}
				log << std::endl;
				log << _socket.bind_addr() << ' ';
				log << msg << " events ";
				log << _poller;
				log << std::endl;
			}

			Remote_server* peer(Endpoint addr, Event::Evs events) {
				// bind to server address with ephemeral port
				Endpoint srv_addr = server_addr();
				srv_addr.port(0);
				Socket sock;
				sock.bind(srv_addr);
				sock.connect(addr);
				return peer(sock, sock.bind_addr(), addr, events);
			}

			Remote_server* peer(Socket sock, Endpoint addr, Endpoint vaddr, Event::Evs events) {
				Remote_server* s = new Remote_server(sock, addr);
				s->vaddr(vaddr);
				_upstream[vaddr] = s;
				_servers[sock] = s;
				_poller.add(Event(events, sock));
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

			int _stop_iterations = 0;

			static const int MAX_STOP_ITERATIONS = 13;
			static const int DEFAULT_EVENTS = POLLRDHUP | POLLIN;
		};

		template<class Kernel, template<class X> class Pool, class Type, class Server_socket>
		struct Remote_Rserver {

			typedef Remote_Rserver<Kernel, Pool, Type, Server_socket> This;
			typedef Kernel_packet<Kernel, Foreign_stream, Type> Packet;
			typedef typename Foreign_stream::Pos Pos;

			Remote_Rserver(Socket sock, Endpoint endpoint):
				_socket(sock),
				_vaddr(endpoint),
				_istream(),
				_ostream(),
				_ipacket(),
				_buffer(),
				_parent(nullptr) {}

			Remote_Rserver(const Remote_Rserver&) = delete;
			Remote_Rserver& operator=(const Remote_Rserver&) = delete;

			Remote_Rserver(Remote_Rserver&& rhs):
				_socket(std::move(rhs._socket)),
				_vaddr(rhs._vaddr),
				_istream(std::move(rhs._istream)),
				_ostream(std::move(rhs._ostream)),
				_ipacket(rhs._ipacket),
				_buffer(std::move(rhs._buffer)) ,
				_parent(rhs._parent) {}

			virtual ~Remote_Rserver() {
				while (!_buffer.empty()) {
					delete _buffer.front();
					_buffer.pop_front();
				}
			}

			void recover_kernels() {

				read_kernels();
//				clear_kernel_buffer(_ostream.global_read_pos());

				Logger(Level::HANDLER)
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;
				
				// recover kernels written to output buffer
				while (!_buffer.empty()) {
					recover_kernel(_buffer.front());
					_buffer.pop_front();
				}
			}

			void send(Kernel* kernel) {
				Logger(Level::HANDLER) << "Remote_Rserver::send()" << std::endl;
				if (kernel->result() == Result::NO_PRINCIPAL_FOUND) {
					Logger(Level::HANDLER) << "poll send error " << _ostream << std::endl;
				}
				if (!kernel->identifiable() && !kernel->moves_everywhere()) {
					kernel->id(factory_generate_id());
					Logger(Level::HANDLER) << "Kernel generate id = " << kernel->id() << std::endl;
				}
				if ((kernel->moves_upstream() || kernel->moves_somewhere()) && kernel->identifiable()) {
					_buffer.push_back(kernel);
					Logger(Level::COMPONENT) << "Buffer size = " << _buffer.size() << std::endl;
				}
				Logger(Level::COMPONENT) << "Sent kernel " << *kernel << std::endl;
				Packet packet;
				packet.write(_ostream, kernel);
			}

			bool valid() const {
//				TODO: It is probably too slow to check error on every event.
				return _socket.error() == 0;
			}

			template<class F>
			void handle_event(Event event, F on_overflow) {
				bool overflow = false;
				if (event.is_reading()) {
					_istream.fill<Server_socket&>(_socket);
					bool state_is_ok = true;
					while (state_is_ok && !_istream.empty()) {
						Logger(Level::HANDLER) << "Recv " << _istream << std::endl;
						try {
							state_is_ok = _ipacket.read(_istream, [this] (Kernel* k) {
								k->from(_vaddr);
								Logger(Level::COMPONENT) << "Received kernel " << *k << std::endl;
								if (k->moves_downstream()) {
									clear_kernel_buffer(k);
								}
							});
						} catch (No_principal_found<Kernel>& err) {
							Logger(Level::HANDLER) << "No principal found for "
								<< int(err.kernel()->result()) << std::endl;
							Kernel* k = err.kernel();
							k->principal(k->parent());
							if (_parent != nullptr) {
								_parent->send(k);
							} else {
								send(k);
							}
							overflow = true;
						}
						if (state_is_ok) {
							_ipacket.reset_reading_state();
						}
					}
				}
				if (event.is_writing() && !event.is_closing()) {
//					try {
						Logger(Level::HANDLER) << "Send " << _ostream << std::endl;
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
						_ostream.flush<Server_socket&>(_socket);
						if (_ostream.empty()) {
							Logger(Level::HANDLER) << "Flushed." << std::endl;
//							clear_kernel_buffer();
							_ostream.reset();
						} else {
//							clear_kernel_buffer(_ostream.global_read_pos());
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

			int fd() const { return _socket; }
			Socket socket() const { return _socket; }
			void socket(Socket rhs) {
				_istream.fill<Server_socket&>(_socket);
				_socket = rhs;
			}
			Endpoint bind_addr() const { return _socket.bind_addr(); }
			Endpoint vaddr() const { return _vaddr; }
			void vaddr(Endpoint rhs) { _vaddr = rhs; }

			bool empty() const { return _buffer.empty(); }

			This* parent() const { return _parent; }
			void parent(This* rhs) { _parent = rhs; }

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << '('
//					<< rhs.bind_addr() << " <--> "
					<< rhs.vaddr() << ','
					<< rhs.fd() << ','
					<< rhs.socket() << ' '
					<< rhs._buffer.size() << ','
					<< rhs._istream.size() << ','
					<< rhs._ostream.size() << ')';
			}

		private:

			static void recover_kernel(Kernel* k) {
				k->from(k->to());
				k->result(Result::ENDPOINT_NOT_CONNECTED);
				k->principal(k->parent());
				factory_send(k);
			}

//			void clear_kernel_buffer() {
//				while (!_buffer.empty()) {
//					Logger(Level::HANDLER) << "sent kernel " << *_buffer.front().second << std::endl;;
//					delete _buffer.front().second;
//					_buffer.pop();
//				}
//			}
//
//			void clear_kernel_buffer(Pos global_read_pos) {
//				while (!_buffer.empty() && _buffer.front().first <= global_read_pos) {
//					Logger(Level::HANDLER) << "sent kernel " << *_buffer.front().second << std::endl;;
//					delete _buffer.front().second;
//					_buffer.pop();
//				}
//			}
			
			void clear_kernel_buffer(Kernel* k) {
				auto pos = std::find_if(_buffer.begin(), _buffer.end(), [k] (Kernel* rhs) {
					return *rhs == *k;
				});
				if (pos != _buffer.end()) {
					Logger(Level::HANDLER) << "Kernel erased " << k->id() << std::endl;
					_buffer.erase(pos);
					Logger(Level::COMPONENT) << "Buffer size = " << _buffer.size() << std::endl;
				} else {
					Logger(Level::HANDLER) << "Kernel not found " << k->id() << std::endl;
				}
			}
			
			void read_kernels() {
				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				handle_event(Event(POLLIN, _socket), [](bool) {});
			}

			Server_socket _socket;
			Endpoint _vaddr;
			Foreign_stream _istream;
			Foreign_stream _ostream;
			Packet _ipacket;
			std::deque<Kernel*> _buffer;

			This* _parent;
		};

	}

}

