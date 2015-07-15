namespace factory {

	namespace components {

		template<class Kernel>
		struct Kernel_packet {

			void write(packstream& out, Kernel& kernel) {
				typedef packstream::pos_type pos_type;
				pos_type old_pos = out.tellp();
				Type<Kernel>::types().write_object(kernel, out);
				pos_type new_pos = out.tellp();
				out << end_packet;
				Logger<Level::COMPONENT>() << "send bytes="
					<< new_pos-old_pos
					<< ",stream="
					<< debug_stream(out)
					<< ",krnl=" << kernel
					<< std::endl;
			}

			template<class F, class G>
			void read(packstream& in, F callback, G callback2) {
				Type<Kernel>::types().read_and_send_object(in, callback, callback2);
			}

		};

		template<class Server, class Remote_server, class Kernel, template<class X> class Pool>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Pool>, Server> {
			
			typedef Socket_server<Server, Remote_server, Kernel, Pool> this_type;
			typedef std::map<Endpoint, Remote_server*> upstream_type;
			typedef std::unordered_map<int, Remote_server*> servers_type;

			Socket_server() = default;

			~Socket_server() {
				std::for_each(_upstream.begin(), _upstream.end(),
					[] (const std::pair<const Endpoint, Remote_server*>& rhs) {
						delete rhs.second;
					}
				);
			}

			void serve() {
				this->process_kernels();
				_poller.run([this] (Event event) {
					bool stopping = this->_stopping;
					if (stopping) {
						event.no_reading();
					}
					Logger<Level::SERVER>()
						<< "Event " << event << std::endl;
					if (event.fd() == _poller.pipe()) {
						Logger<Level::SERVER>() << "Notification " << event << std::endl;
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
								Logger<Level::SERVER>()
									<< "connected peer " << s->vaddr() << std::endl;
							} else {
								Remote_server* s = res->second;
								Logger<Level::SERVER>()
									<< "ports: "
									<< addr.port() << ' '
									<< s->bind_addr().port()
									<< std::endl;
								if (addr.port() < s->bind_addr().port()) {
									Logger<Level::SERVER> log;
									log << "not replacing peer " << *s
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
									Logger<Level::SERVER> log;
									log << "replacing peer " << *s;
									_poller.ignore(s->fd());
									_servers.erase(s->fd());
									Remote_server* new_s = new Remote_server(std::move(*s));
									new_s->socket(sock);
									_servers[sock] = new_s;
									_upstream[vaddr] = new_s;
									Event ev(DEFAULT_EVENTS | Event::Out, sock);
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
							Logger<Level::SERVER>() << "Invalid socket" << std::endl;
							erasing = true;
						} else {
							auto res = _servers.find(event.fd());
							if (res == _servers.end()) {
								debug("qqq");
								std::stringstream msg;
								msg << this_process::id() << ' ' << this->server_addr() << ' ';
								msg << " can not find server to process event: fd=" << event.fd();
								Logger<Level::SERVER>() << msg.str() << std::endl;
								throw Error(msg.str(), __FILE__, __LINE__, __func__);
							}
							Remote_server* s = res->second;
							if (!s->valid()) {
								Logger<Level::SERVER>() << "Invalid socket" << std::endl;
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
					if (stopping) {
						process_kernels();
						flush_kernels();
						++_stop_iterations;
						if (this->empty() || _stop_iterations == MAX_STOP_ITERATIONS) {
							debug("stopping");
							_poller.stop();
							this->_stopping = false;
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

			bool stopping() const { return this->_stopping; }

			void send(Kernel* kernel) {
//				if (this->stopped()) {
//					throw Error("Can not send kernel when the server is stopped.",
//						__FILE__, __LINE__, __func__);
//				}
				std::unique_lock<std::mutex> lock(_mutex);
				this->_pool.push(kernel);
				lock.unlock();
				this->_poller.notify();
			}

			void peer(Endpoint addr) {
//				if (!this->stopped()) {
//					throw Error("Can not add upstream server when socket server is running.",
//						__FILE__, __LINE__, __func__);
//				}
				this->peer(addr, DEFAULT_EVENTS);
			}

			void erase(Endpoint addr) {
				Logger<Level::SERVER>() << "Removing " << addr << std::endl;
				auto res = _upstream.find(addr);
				if (res == _upstream.end()) {
					std::stringstream msg;
					msg << "Can not find server to erase: " << addr;
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				Remote_server* s = res->second;
				_upstream.erase(res);
				_servers.erase(s->fd());
				delete s;
			}

			void socket(Endpoint addr) {
				this->_socket.bind(addr);
				this->_socket.listen();
				this->_poller.add(Event(DEFAULT_EVENTS, this->_socket));
			}

			void start() {
				Logger<Level::SERVER>() << "Socket_server::start()" << std::endl;
				this->_thread = std::thread(&this_type::serve, this);
			}
	
			void stop_impl() {
				Logger<Level::SERVER>() << "Socket_server::stop_impl()" << std::endl;
				this->_stopping = true;
				this->_poller.notify();
			}

			void wait_impl() {
				Logger<Level::SERVER>() << "Socket_server::wait_impl()" << std::endl;
				if (this->_thread.joinable()) {
					this->_thread.join();
				}
				Logger<Level::SERVER>() << "Socket_server::wait_impl() end" << std::endl;
			}

			void affinity(int cpu) { _cpu = cpu; }

			friend std::ostream& operator<<(std::ostream& out, const this_type& rhs) {
				return out << "sserver " << rhs._cpu;
			}

			Endpoint server_addr() const { return this->_socket.bind_addr(); }
		
		private:

			Endpoint virtual_addr(Endpoint addr) const {
				Endpoint vaddr = addr;
				vaddr.port(server_addr().port());
				return vaddr;
			}

			void erase(int fd) {
				auto r = _servers.find(fd);
				if (r == _servers.end()) {
					return;
//					std::stringstream msg;
//					msg << "Can not find server to erase: fd=" << fd;
//					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				Remote_server* s = r->second;
				Logger<Level::SERVER>() << "Removing server " << *s << std::endl;
				s->recover_kernels(this->parent());
				// subordinate servers are not present in upstream
				if (!s->parent()) {
					_upstream.erase(s->vaddr());
				}
				_servers.erase(fd);
				delete s;
			}
	

			void process_event(Remote_server* server, Event event) {
				server->handle_event(event, this->parent(),
					[this, &event, server] (bool overflow) {
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
					}
				);
			}

			void flush_kernels() {
				for (auto pair : _upstream) {
					Remote_server* server = pair.second;
					process_event(server, Event(Event::Out, server->fd()));
				}
			}

			void process_kernels() {
				Logger<Level::SERVER>() << "Socket_server::process_kernels()" << std::endl;
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

					if (this->server_addr() && k->to() == this->server_addr()) {
						std::ostringstream msg;
						msg << "Kernel is sent to local node. From="
							<< this->server_addr() << ", to=" << k->to();
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}

					if (k->moves_everywhere()) {
						Logger<Level::SERVER>() << "broadcast kernel" << std::endl;
						for (auto pair : _upstream) {
							Remote_server* s = pair.second;
							s->send(k);
							_poller[s->fd()]->writing();
						}
						// delete broadcast kernel
						delete k;
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
							Remote_server* handler = peer(k->to(), DEFAULT_EVENTS | Event::Out);
							handler->send(k);
						} else {
							result->second->send(k);
							_poller[result->second->fd()]->writing();
						}
					}
				}
			}

			template<class M>
			struct Print_values {
				typedef M map_type;
				typedef typename map_type::key_type key_type;
				typedef typename map_type::value_type value_type;
				Print_values(const M& m): map(m) {}
				friend std::ostream& operator<<(std::ostream& out, const Print_values& rhs) {
					out << '{';
					intersperse_iterator<Remote_server> it(out, ",");
					std::transform(rhs.map.begin(), rhs.map.end(), it,
						[] (const value_type& pair) -> const Remote_server& {
							return *pair.second;
						}
					);
					out << '}';
					return out;
				}
			private:
				const M& map;
			};

			template<class M>
			Print_values<M> print_values(const M& m) { return Print_values<M>(m); }

			void debug(const char* msg = "") {
				Logger<Level::SERVER>()
					<< msg << " upstream " << print_values(this->_upstream) << std::endl
					<< msg << " servers " << print_values(this->_servers) << std::endl
					<< msg << " events " << this->_poller << std::endl;
			}

			Remote_server* peer(Endpoint addr, Event::legacy_event events) {
				// bind to server address with ephemeral port
				Endpoint srv_addr = this->server_addr();
				srv_addr.port(0);
				Socket sock;
				sock.bind(srv_addr);
				sock.connect(addr);
				return peer(sock, sock.bind_addr(), addr, events);
			}

			Remote_server* peer(Socket sock, Endpoint addr, Endpoint vaddr, Event::legacy_event events) {
				Remote_server* s = new Remote_server(sock, addr);
				s->vaddr(vaddr);
				_upstream[vaddr] = s;
				_servers[sock] = s;
				_poller.add(Event(events, sock));
				return s;
			}

			Poller _poller;
			Server_socket _socket;
			upstream_type _upstream;
			servers_type _servers;
			Pool<Kernel*> _pool;

			// multi-threading
			int _cpu = 0;
			std::thread _thread;
			std::mutex _mutex;

			volatile bool _stopping = false;
			int _stop_iterations = 0;

			static const int MAX_STOP_ITERATIONS = 13;
			static const Event::legacy_event DEFAULT_EVENTS = Event::Hup | Event::In;
		};

		template<class Kernel, template<class X> class Pool, class Server_socket>
		struct Remote_Rserver {

			typedef Remote_Rserver<Kernel, Pool, Server_socket> this_type;
			typedef Kernel_packet<Kernel> Packet;
			typedef char Ch;
			typedef basic_kernelbuf<basic_fdbuf<Ch,Server_socket>> Kernelbuf;
			typedef Packing_stream<char> Stream;

			Remote_Rserver(Socket sock, Endpoint endpoint):
				_vaddr(endpoint),
				_kernelbuf(),
				_stream(&this->_kernelbuf),
				_ipacket(),
				_buffer(),
				_parent(nullptr)
			{
				this->_kernelbuf.setfd(sock);
			}

			Remote_Rserver(const Remote_Rserver&) = delete;
			Remote_Rserver& operator=(const Remote_Rserver&) = delete;

			Remote_Rserver(Remote_Rserver&& rhs):
				_vaddr(rhs._vaddr),
				_kernelbuf(std::move(rhs._kernelbuf)),
				_stream(std::move(rhs._stream)),
				_ipacket(rhs._ipacket),
				_buffer(std::move(rhs._buffer)) ,
				_parent(rhs._parent) {}

			virtual ~Remote_Rserver() {
				while (!_buffer.empty()) {
					delete _buffer.front();
					_buffer.pop_front();
				}
			}

			void recover_kernels(Server<Kernel>* parent_server) {

				read_kernels(parent_server);

				Logger<Level::HANDLER>()
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;
				
				// recover kernels written to output buffer
				while (!_buffer.empty()) {
					recover_kernel(_buffer.front(), parent_server);
					_buffer.pop_front();
				}
			}

			void send(Kernel* kernel) {
				Logger<Level::HANDLER>() << "Remote_Rserver::send()" << std::endl;
				if (kernel->result() == Result::NO_PRINCIPAL_FOUND) {
					Logger<Level::HANDLER>() << "poll send error: tellp=" << _stream.tellp() << std::endl;
				}
				bool erase_kernel = true;
				if (!kernel->identifiable() && !kernel->moves_everywhere()) {
					kernel->id(factory_generate_id());
					erase_kernel = false;
					Logger<Level::HANDLER>() << "Kernel generate id = " << kernel->id() << std::endl;
				}
				if ((kernel->moves_upstream() || kernel->moves_somewhere()) && kernel->identifiable()) {
					_buffer.push_back(kernel);
					erase_kernel = false;
					Logger<Level::COMPONENT>() << "Buffer size = " << _buffer.size() << std::endl;
				}
				Packet packet;
				packet.write(_stream, *kernel);
				if (erase_kernel && !kernel->moves_everywhere()) {
					Logger<Level::COMPONENT>() << "Delete kernel " << *kernel << std::endl;
					delete kernel;
				}
			}

			bool valid() const {
//				TODO: It is probably too slow to check error on every event.
				return this->socket().error() == 0;
			}

			template<class F>
			void handle_event(Event event, Server<Kernel>* parent_server, F on_overflow) {
				bool overflow = false;
				if (event.is_reading()) {
					Logger<Level::COMPONENT>() << "recv rdstate="
						<< debug_stream(_stream) << ",event=" << event << std::endl;
					while (this->_stream) {
						try {
							_ipacket.read(this->_stream, [this] (Kernel* k) {
								k->from(_vaddr);
								Logger<Level::COMPONENT>()
									<< "recv kernel=" << *k
									<< ",rdstate=" << debug_stream(this->_stream)
									<< std::endl;
								if (k->moves_downstream()) {
									this->clear_kernel_buffer(k);
								}
							}, [parent_server] (Kernel* k) {
								parent_server->send(k);
							});
						} catch (No_principal_found<Kernel>& err) {
							Logger<Level::HANDLER>() << "No principal found for "
								<< err.kernel()->result() << std::endl;
							Kernel* k = err.kernel();
							k->principal(k->parent());
							if (_parent != nullptr) {
								_parent->send(k);
							} else {
								send(k);
							}
							overflow = true;
						}
					}
					this->_stream.clear();
				}
				if (event.is_writing() && !event.is_closing()) {
					Logger<Level::HANDLER>() << "Send rdstate=" << debug_stream(this->_stream) << std::endl;
					this->_stream.flush();
					this->socket().flush();
					if (this->_stream) {
						Logger<Level::COMPONENT>() << "Flushed." << std::endl;
					}
					if (!this->_stream || !this->socket().empty()) {
						overflow = true;
						this->_stream.clear();
					}
				}
				on_overflow(overflow);
			}

			int fd() const { return this->socket(); }
			const Server_socket& socket() const { return this->_kernelbuf.fd(); }
			Server_socket& socket() { return this->_kernelbuf.fd(); }
			void socket(Socket rhs) {
				this->_stream >> underflow;
				this->_stream.clear();
				this->_kernelbuf.setfd(rhs);
			}
			Endpoint bind_addr() const { return this->socket().bind_addr(); }
			Endpoint vaddr() const { return _vaddr; }
			void vaddr(Endpoint rhs) { _vaddr = rhs; }

			bool empty() const { return this->_buffer.empty(); }

			this_type* parent() const { return this->_parent; }
			void parent(this_type* rhs) { this->_parent = rhs; }

			friend std::ostream& operator<<(std::ostream& out, const this_type& rhs) {
				return out << "{vaddr="
					<< rhs.vaddr() << ",sock="
					<< rhs.socket() << ",kernels="
					<< rhs._buffer.size() << ",str="
					<< debug_stream(rhs._stream) << '}';
			}

		private:

			void recover_kernel(Kernel* k, Server<Kernel>* parent_server) {
				k->from(k->to());
				k->result(Result::ENDPOINT_NOT_CONNECTED);
				k->principal(k->parent());
				parent_server->send(k);
			}

			void clear_kernel_buffer(Kernel* k) {
				auto pos = std::find_if(_buffer.begin(), _buffer.end(), [k] (Kernel* rhs) {
					return *rhs == *k;
				});
				if (pos != _buffer.end()) {
					Logger<Level::COMPONENT>() << "Kernel erased " << k->id() << std::endl;
					delete *pos;
					this->_buffer.erase(pos);
					Logger<Level::COMPONENT>() << "Buffer size = " << _buffer.size() << std::endl;
				} else {
					Logger<Level::COMPONENT>() << "Kernel not found " << k->id() << std::endl;
				}
			}
			
			void read_kernels(Server<Kernel>* parent_server) {
				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				handle_event(Event(Event::In, this->fd()), parent_server, [](bool) {});
			}

			Endpoint _vaddr;
			Kernelbuf _kernelbuf;
			Stream _stream;
			Packet _ipacket;
			std::deque<Kernel*> _buffer;

			this_type* _parent;
		};

	}

}

