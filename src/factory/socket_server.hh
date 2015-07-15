namespace factory {

	namespace components {

		template<class Server, class Remote_server, class Kernel, template<class X> class Pool>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Pool>, Server> {
			
			typedef Socket_server<Server, Remote_server, Kernel, Pool> this_type;
			typedef std::map<Endpoint, Remote_server*> upstream_type;
			typedef std::unordered_map<int, Remote_server*> servers_type;
			typedef std::mutex mutex_type;
			typedef std::thread thread_type;
			typedef Remote_server server_type;

			Socket_server() = default;

			~Socket_server() {
				std::for_each(_upstream.begin(), _upstream.end(),
					[] (const std::pair<const Endpoint, server_type*>& rhs) {
						delete rhs.second;
					}
				);
			}

			void serve() {
				this->process_kernels();
				this->_poller.run([this] (Event& event) {
					bool stopping = this->_stopping;
					if (stopping) {
						event.unsetrev(Event::In);
					}
					Logger<Level::SERVER>()
						<< "Event " << event << std::endl;
					if (event.fd() == this->_poller.pipe()) {
						Logger<Level::SERVER>() << "Notification " << event << std::endl;
						this->process_kernels();
					} else if (event.fd() == this->_socket.fd()) {
						if (event.in()) {
							this->accept_connection();
						}
					} else {
						this->process_event(event);
					}
					if (stopping) {
						this->try_to_stop_gracefully();
					}
				});
			}

			void accept_connection() {
				auto pair = _socket.accept();
				Socket sock = pair.first;
				Endpoint addr = pair.second;
				Endpoint vaddr = virtual_addr(addr);
//				Endpoint vaddr = addr;
				auto res = _upstream.find(vaddr);
				if (res == _upstream.end()) {
					server_type* s = peer(sock, addr, vaddr, DEFAULT_EVENTS);
					Logger<Level::SERVER>()
						<< "connected peer " << s->vaddr() << std::endl;
				} else {
					server_type* s = res->second;
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
						server_type* new_s = new server_type(sock, addr);
						new_s->vaddr(vaddr);
						new_s->parent(s);
						_servers[sock] = new_s;
						_poller.add(Event(DEFAULT_EVENTS, sock));
//						sock.unsetrev(Event::In);
//						s->fill_from(sock);
//						sock.close();
						debug("not replacing upstream");
					} else {
						Logger<Level::SERVER> log;
						log << "replacing peer " << *s;
						_poller.disable(s->fd());
						_servers.erase(s->fd());
						server_type* new_s = new server_type(std::move(*s));
						new_s->socket(sock);
						_servers[sock] = new_s;
						_upstream[vaddr] = new_s;
						
						this->_poller.add(Event(sock, ALL_EVENTS, ALL_EVENTS));
//						erase(s->bind_addr());
//						peer(sock, addr, vaddr, DEFAULT_EVENTS);
						log << " with " << *new_s << std::endl;
						debug("replacing upstream");
						delete s;
					}
				}
			}

			void process_event(Event& event) {
				bool erasing = false;
				if (event.err()) {
					Logger<Level::SERVER>() << "Invalid socket" << std::endl;
					erasing = true;
				} else {
					auto res = this->_servers.find(event.fd());
					if (res == this->_servers.end()) {
						std::stringstream msg;
						msg << this->server_addr()
							<< " can not find server to process event: fd="
							<< event.fd();
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}
					server_type* s = res->second;
					if (!s->valid()) {
						Logger<Level::SERVER>() << "Invalid socket" << std::endl;
						erasing = true;
					} else {
						s->handle_event(event, this->parent());
						erasing = event.bad();
					}
				}
				if (erasing) {
					this->erase(event.fd());
				}
			}

			void try_to_stop_gracefully() {
				this->process_kernels();
				this->flush_kernels();
				++this->_stop_iterations;
				if (this->empty() || this->_stop_iterations == MAX_STOP_ITERATIONS) {
					debug("stopping");
					this->_poller.stop();
					this->_stopping = false;
				} else {
					debug("not stopping");
				}
			}

			bool empty() const {
				return std::all_of(_upstream.cbegin(), _upstream.cend(),
					[] (const std::pair<Endpoint,server_type*>& rhs)
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
				std::unique_lock<mutex_type> lock(_mutex);
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
				server_type* s = res->second;
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
				this->_thread = thread_type(&this_type::serve, this);
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
				server_type* s = r->second;
				Logger<Level::SERVER>() << "Removing server " << *s << std::endl;
				s->recover_kernels(this->parent());
				// subordinate servers are not present in upstream
				if (!s->parent()) {
					_upstream.erase(s->vaddr());
				}
				_servers.erase(fd);
				delete s;
			}
	
			void flush_kernels() {
				for (auto pair : _upstream) {
					server_type* server = pair.second;
					Event ev(Event::Out, server->fd());
					server->handle_event(ev, this->parent());
				}
			}

			void process_kernels() {
				Logger<Level::SERVER>() << "Socket_server::process_kernels()" << std::endl;
				bool pool_is_empty = false;
				{
					std::unique_lock<mutex_type> lock(_mutex);
					pool_is_empty = _pool.empty();
				}
				while (!pool_is_empty) {

					std::unique_lock<mutex_type> lock(_mutex);
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
							server_type* s = pair.second;
							s->send(k);
							_poller[s->fd()]->setall(Event::Out);
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
						_poller[result->second->fd()]->setall(Event::Out);
					} else {
						// create endpoint if necessary, and send kernel
						if (k->to() == Endpoint()) {
							k->to(k->from());
						}
						auto result = _upstream.find(k->to());
						if (result == _upstream.end()) {
							server_type* handler = peer(k->to(), DEFAULT_EVENTS | Event::Out);
							handler->send(k);
						} else {
							result->second->send(k);
							_poller[result->second->fd()]->setall(Event::Out);
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
					intersperse_iterator<server_type> it(out, ",");
					std::transform(rhs.map.begin(), rhs.map.end(), it,
						[] (const value_type& pair) -> const server_type& {
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

			server_type* peer(Endpoint addr, Event::legacy_event events) {
				// bind to server address with ephemeral port
				Endpoint srv_addr = this->server_addr();
				srv_addr.port(0);
				Socket sock;
				sock.bind(srv_addr);
				sock.connect(addr);
				return peer(sock, sock.bind_addr(), addr, events);
			}

			server_type* peer(Socket sock, Endpoint addr, Endpoint vaddr, Event::legacy_event events) {
				server_type* s = new server_type(sock, addr);
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
			thread_type _thread;
			mutable mutex_type _mutex;

			volatile bool _stopping = false;
			int _stop_iterations = 0;

			static const int MAX_STOP_ITERATIONS = 13;
			static const Event::legacy_event DEFAULT_EVENTS = Event::Hup | Event::In;
			static const Event::legacy_event ALL_EVENTS = DEFAULT_EVENTS | Event::Out;
		};

		template<class Kernel, template<class X> class Pool, class Server_socket>
		struct Remote_Rserver {

			typedef Remote_Rserver<Kernel, Pool, Server_socket> this_type;
			typedef char Ch;
			typedef basic_kernelbuf<basic_fdbuf<Ch,Server_socket>> Kernelbuf;
			typedef Packing_stream<char> Stream;

			Remote_Rserver(Socket sock, Endpoint endpoint):
				_vaddr(endpoint),
				_kernelbuf(),
				_stream(&this->_kernelbuf),
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
				this->write_kernel(*kernel);
				if (erase_kernel && !kernel->moves_everywhere()) {
					Logger<Level::COMPONENT>() << "Delete kernel " << *kernel << std::endl;
					delete kernel;
				}
			}

			bool valid() const {
//				TODO: It is probably too slow to check error on every event.
				return this->socket().error() == 0;
			}

			void handle_event(Event& event, Server<Kernel>* parent_server) {
				bool overflow = false;
				if (event.in()) {
					Logger<Level::COMPONENT>() << "recv rdstate="
						<< debug_stream(_stream) << ",event=" << event << std::endl;
					while (this->_stream) {
						try {
							this->read_and_send_kernel(parent_server);
						} catch (No_principal_found<Kernel>& err) {
							this->return_kernel(err.kernel());
							overflow = true;
						}
					}
					this->_stream.clear();
				}
				if (event.out() && !event.hup()) {
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
				if (overflow) {
					event.setev(Event::Out);
//					// Failed kernels are sent to parent,
//					// so we need to fire write event.
//					if (this->parent() != nullptr) {
//						_poller[server->parent()->fd()]->setall(Event::Out);
//					}
				} else {
					event.unsetev(Event::Out);
				}
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

			void read_and_send_kernel(Server<Kernel>* parent_server) {
				Type<Kernel>::read_object(this->_stream, [this] (Kernel* k) {
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
			}

			void return_kernel(Kernel* k) {
				Logger<Level::HANDLER>() << "No principal found for "
					<< *k << std::endl;
				k->principal(k->parent());
				this->send(k);
				// TODO: strange logic
//				if (this->parent() != nullptr) {
//					this->parent()->send(k);
//				} else {
//				this->send(k);
//				}
			}

			void write_kernel(Kernel& kernel) {
				typedef packstream::pos_type pos_type;
				pos_type old_pos = this->_stream.tellp();
				Type<Kernel>::write_object(kernel, this->_stream);
				pos_type new_pos = this->_stream.tellp();
				this->_stream << end_packet;
				Logger<Level::COMPONENT>() << "send bytes="
					<< new_pos-old_pos
					<< ",stream="
					<< debug_stream(this->_stream)
					<< ",krnl=" << kernel
					<< std::endl;
			}

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
				Event ev(Event::In, this->fd());
				handle_event(ev, parent_server);
			}

			Endpoint _vaddr;
			Kernelbuf _kernelbuf;
			Stream _stream;
			std::deque<Kernel*> _buffer;

			this_type* _parent;
		};

	}

}

