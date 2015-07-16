namespace factory {

	namespace components {

		template<class Server, class Remote_server, class Kernel, template<class X> class Pool>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Pool>, Server> {

			typedef Remote_server server_type;
			typedef Socket_server<Server, Remote_server, Kernel, Pool> this_type;
			typedef std::map<Endpoint, Remote_server*> upstream_type;
			typedef std::mutex mutex_type;
			typedef std::thread thread_type;
			typedef Server_socket socket_type;
			typedef Pool<Kernel*> pool_type;

			struct server_deleter {
				server_deleter(): _this(nullptr) {}
				explicit server_deleter(this_type* t): _this(t) {}
				void operator()(server_type* ptr) {
					if (_this) {
						auto it = _this->_upstream.find(ptr->vaddr());
						if (it != _this->_upstream.end()) {
							_this->_upstream.erase(it);
						}
						Logger<Level::SERVER>() << "Removing server " << *ptr << std::endl;
						ptr->recover_kernels(_this->parent());
//						// subordinate servers are not present in upstream
//						if (!ptr->parent()) {
//							_this->_upstream.erase(ptr->vaddr());
//						}
					}
					delete ptr;
				}
				this_type* _this;
			};

			typedef Poller<server_type, server_deleter> poller_type;

			Socket_server() = default;
			virtual ~Socket_server() = default;

			void serve() {
				// start processing as early as possible
				this->process_kernels();
				this->_poller.run(std::bind(&this_type::handle_event,
					this, std::placeholders::_1));
				// prevent double free or corruption
				this->_poller.clear();
			}

			void handle_event(Event& ev) {
				if (ev.fd() == this->_poller.pipe_in()) {
					this->process_kernels();
					if (this->stopping()) {
						this->try_to_stop_gracefully();
					}
				} else if (ev.fd() == this->_socket.fd()) {
					if (ev.in()) {
						this->accept_connection();
					}
				}
			}

			void accept_connection() {
				auto pair = this->_socket.accept();
				Socket sock = pair.first;
				Endpoint addr = pair.second;
				Endpoint vaddr = virtual_addr(addr);
//				Endpoint vaddr = addr;
				auto res = _upstream.find(vaddr);
				if (res == _upstream.end()) {
					server_type* s = peer(sock, addr, vaddr, Event::In);
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
						server_type* new_s = new server_type(sock, addr, this->parent());
						new_s->vaddr(vaddr);
						new_s->parent(s);
						this->_poller.add(Event{sock, Event::In}, new_s, server_deleter(this));
//						sock.unsetrev(Event::In);
//						s->fill_from(sock);
//						sock.close();
						debug("not replacing upstream");
					} else {
						Logger<Level::SERVER> log;
						log << "replacing peer " << *s;
						_poller.disable(s->fd());
						server_type* new_s = new server_type(std::move(*s));
						new_s->socket(sock);
						_upstream[vaddr] = new_s;
						
						this->_poller.add(Event{sock, Event::Inout, Event::Inout}, new_s, server_deleter(this));
//						erase(s->bind_addr());
//						peer(sock, addr, vaddr, Event::In);
						log << " with " << *new_s << std::endl;
						debug("replacing upstream");
//						delete s;
					}
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
				this->peer(addr, Event::In);
			}

			void socket(Endpoint addr) {
				this->_socket.bind(addr);
				this->_socket.listen();
				this->_poller.add(Event{this->_socket, Event::In}, nullptr, server_deleter(this));
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

			void flush_kernels() {
				typedef typename upstream_type::value_type pair_type;
				std::for_each(this->_upstream.begin(),
					this->_upstream.end(), [] (pair_type& rhs)
					{ rhs.second->simulate(Event{rhs.second->fd(), Event::Out}); });
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
//							_poller[s->fd()]->setall(Event::Out);
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
//						_poller[result->second->fd()]->setall(Event::Out);
					} else {
						// create endpoint if necessary, and send kernel
						if (!k->to()) {
							k->to(k->from());
						}
						auto result = _upstream.find(k->to());
						if (result == _upstream.end()) {
							server_type* handler = peer(k->to(), Event::Inout);
							handler->send(k);
						} else {
							result->second->send(k);
//							_poller[result->second->fd()]->setall(Event::Out);
						}
					}
				}
			}

			void debug(const char* msg = "") {
				Logger<Level::SERVER>()
					<< msg << " upstream " << print_values(this->_upstream) << std::endl
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
				server_type* s = new server_type(sock, addr, this->parent());
				s->vaddr(vaddr);
				_upstream[vaddr] = s;
				_poller.add(Event{sock, events}, s, server_deleter(this));
				return s;
			}

			poller_type _poller;
			socket_type _socket;
			pool_type _pool;
			upstream_type _upstream;

			// multi-threading
			int _cpu = 0;
			thread_type _thread;
			mutable mutex_type _mutex;

			volatile bool _stopping = false;
			int _stop_iterations = 0;

			static const int MAX_STOP_ITERATIONS = 13;
		};

		template<class Kernel, template<class X> class Pool, class Server_socket>
		struct Remote_Rserver {

			typedef Remote_Rserver<Kernel, Pool, Server_socket> this_type;
			typedef char Ch;
			typedef basic_kernelbuf<basic_fdbuf<Ch,Server_socket>> Kernelbuf;
			typedef Packing_stream<char> Stream;

			Remote_Rserver(Socket sock, Endpoint endpoint, Server<Kernel>* fac):
				_vaddr(endpoint),
				_kernelbuf(),
				_stream(&this->_kernelbuf),
				_buffer(),
				_parent(nullptr),
				_factory(fac),
				_dirty(false)
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
				_parent(rhs._parent),
				_factory(rhs._factory),
				_dirty(rhs._dirty)
				{}

			virtual ~Remote_Rserver() {
				Logger<Level::SERVER>() << "deleting server " << *this << std::endl;
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
				this->_dirty = true;
				if (erase_kernel && !kernel->moves_everywhere()) {
					Logger<Level::COMPONENT>() << "Delete kernel " << *kernel << std::endl;
					delete kernel;
				}
			}

			bool valid() const {
//				TODO: It is probably too slow to check error on every event.
				return this->socket().error() == 0;
			}

			void operator()(Event& event) {
				if (this->valid()) {
					this->handle_event(event, this->_factory);
				} else {
					event.setrev(Event::Hup);
				}
			}

			void simulate(Event event) {
				this->handle_event(event, this->_factory);
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
					this->_dirty = false;
				}
			}

			bool dirty() const { return this->_dirty; }
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
				Event ev{this->fd(), Event::In};
				handle_event(ev, parent_server);
			}

			Endpoint _vaddr;
			Kernelbuf _kernelbuf;
			Stream _stream;
			std::deque<Kernel*> _buffer;

			this_type* _parent;
			Server<Kernel>* _factory;
			volatile bool _dirty = false;
		};

	}

}

