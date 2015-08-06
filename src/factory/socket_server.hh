namespace factory {

	namespace components {

		template<class Server, class Remote_server, class Kernel, template<class X> class Pool>
		struct Socket_server: public Server_link<Socket_server<Server, Remote_server, Kernel, Pool>, Server> {

			typedef Remote_server server_type;
			typedef Socket_server<Server, Remote_server, Kernel, Pool> this_type;
			typedef std::map<Endpoint, Remote_server*> upstream_type;
			typedef std::mutex mutex_type;
			typedef std::thread thread_type;
			typedef Socket socket_type;
			typedef Pool<Kernel*> pool_type;

			struct server_deleter {
				server_deleter(): _this(nullptr) {}
				explicit server_deleter(this_type* t): _this(t) {}
				void operator()(server_type* ptr) {
					if (_this) {
						// remove from the mapping if it is not linked
						// with other subordinate server
						if (!ptr->link()) {
							_this->_upstream.erase(ptr->vaddr());
						}
						Logger<Level::SERVER>() << "Removing server " << *ptr << std::endl;
						ptr->recover_kernels();
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
				Endpoint addr;
				socket_type sock;
				this->_socket.accept(sock, addr);
				Endpoint vaddr = virtual_addr(addr);
				Logger<Level::SERVER>()
					<< "after accept: socket="
					<< sock << std::endl;
				auto res = this->_upstream.find(vaddr);
				if (res == this->_upstream.end()) {
					this->add_connected_server(std::move(sock), vaddr, Event::In);
					Logger<Level::SERVER>()
						<< "connected peer "
						<< vaddr << std::endl;
				} else {
					server_type* s = res->second;
					Logger<Level::SERVER>()
						<< "ports: "
						<< addr.port() << ' '
						<< s->bind_addr().port()
						<< std::endl;
					if (addr.port() < s->bind_addr().port()) {
						Logger<Level::SERVER>()
							<< "not replacing peer " << *s
							<< std::endl;
						// create temporary subordinate server
						// to read kernels until the socket
						// is closed from the other end
						server_type* new_s = new server_type(std::move(sock), addr);
						this->link_server(s, new_s, Event{new_s->fd(), Event::In});
						debug("not replacing upstream");
					} else {
						Logger<Level::SERVER> log;
						log << "replacing peer " << *s;
						this->_poller.disable(s->fd());
						server_type* new_s = new server_type(std::move(*s));
						new_s->setparent(this);
						new_s->socket(std::move(sock));
						this->_upstream[vaddr] = new_s;
						this->_poller.add(Event{new_s->fd(), Event::Inout, Event::Inout}, new_s, server_deleter(this));
						log << " with " << *new_s << std::endl;
						debug("replacing upstream");
					}
				}
				Logger<Level::SERVER>()
					<< "after add: socket="
					<< sock << std::endl;
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
				this->connect_to_server(addr, Event::In);
			}

			void socket(const Endpoint& addr) {
				this->_socket.bind(addr);
				this->_socket.listen();
				this->_poller.add(Event{this->_socket.fd(), Event::In}, nullptr, server_deleter(this));
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
						for (const auto& pair : _upstream) {
							pair.second->send(k);
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
					} else {
						// create endpoint if necessary, and send kernel
						if (!k->to()) {
							k->to(k->from());
						}
						this->find_or_create_peer(k->to(), Event::Inout)->send(k);
					}
				}
			}

			server_type* find_or_create_peer(const Endpoint& addr, Event::legacy_event ev) {
				server_type* ret;
				auto result = _upstream.find(addr);
				if (result == _upstream.end()) {
					ret = this->connect_to_server(addr, ev);
				} else {
					ret = result->second;
				}
				return ret;
			}

			void debug(const char* msg = "") {
				Logger<Level::SERVER>()
					<< msg << " upstream " << print_values(this->_upstream) << std::endl
					<< msg << " events " << this->_poller << std::endl;
			}

			server_type* connect_to_server(Endpoint addr, Event::legacy_event events) {
				// bind to server address with ephemeral port
				Endpoint srv_addr = this->server_addr();
				srv_addr.port(0);
				socket_type sock;
				sock.bind(srv_addr);
				sock.connect(addr);
				return this->add_connected_server(std::move(sock), addr, events);
			}

			server_type* add_connected_server(socket_type&& sock, Endpoint vaddr,
				Event::legacy_event events,
				Event::legacy_event revents=0)
			{
				socket_type::fd_type fd = sock.fd();
				server_type* s = new server_type(std::move(sock), vaddr);
				s->setparent(this);
				this->_upstream[vaddr] = s;
				this->_poller.add(Event{fd, events, revents}, s, server_deleter(this));
				return s;
			}

			void link_server(server_type* par, server_type* sub, Event ev) {
				sub->setparent(this);
				sub->setvaddr(par->vaddr());
				sub->link(par);
				this->_poller.add(ev, sub, server_deleter(this));
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

		template<class Kernel, template<class X> class Pool, class Sock>
		struct Remote_Rserver: public Server<Kernel> {

			typedef Remote_Rserver<Kernel, Pool, Sock> this_type;
			typedef char Ch;
			typedef basic_kernelbuf<basic_fdbuf<Ch,Sock>> Kernelbuf;
			typedef Packing_stream<Ch> stream_type;
			typedef Server<Kernel> server_type;
			typedef Sock socket_type;
			typedef Kernel kernel_type;
			typedef typename Kernel::app_type app_type;
			typedef std::deque<Kernel*> pool_type;

			Remote_Rserver(Socket&& sock, Endpoint vaddr):
				_vaddr(vaddr),
				_kernelbuf(),
				_stream(&this->_kernelbuf),
				_buffer(),
				_link(nullptr),
				_dirty(false)
			{
				this->_kernelbuf.setfd(socket_type(std::move(sock)));
			}

			Remote_Rserver(const Remote_Rserver&) = delete;
			Remote_Rserver& operator=(const Remote_Rserver&) = delete;

			Remote_Rserver(Remote_Rserver&& rhs):
				_vaddr(rhs._vaddr),
				_kernelbuf(std::move(rhs._kernelbuf)),
				_stream(std::move(rhs._stream)),
				_buffer(std::move(rhs._buffer)) ,
				_link(rhs._link),
				_dirty(rhs._dirty)
				{}

			virtual ~Remote_Rserver() {
				Logger<Level::SERVER>() << "deleting server " << *this << std::endl;
				while (!_buffer.empty()) {
					delete _buffer.front();
					_buffer.pop_front();
				}
			}

			void recover_kernels() {

				this->read_kernels();

				Logger<Level::HANDLER>()
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;
				
				// recover kernels written to output buffer
				while (!this->_buffer.empty()) {
					this->recover_kernel(this->_buffer.front(), this->root());
					this->_buffer.pop_front();
				}
			}

			void send(kernel_type* kernel) {
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
					this->handle_event(event);
				} else {
					event.setrev(Event::Hup);
				}
			}

			void simulate(Event event) {
				this->handle_event(event);
			}

			void handle_event(Event& event) {
				bool overflow = false;
				if (event.in()) {
					Logger<Level::COMPONENT>() << "recv rdstate="
						<< debug_stream(_stream) << ",event=" << event << std::endl;
					while (this->_stream) {
						try {
							this->read_and_send_kernel();
						} catch (No_principal_found<kernel_type>& err) {
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
				} else {
					event.unsetev(Event::Out);
					this->_dirty = false;
				}
			}

			bool dirty() const { return this->_dirty; }
			int fd() const { return this->socket().fd(); }
			const socket_type& socket() const { return this->_kernelbuf.fd(); }
			socket_type& socket() { return this->_kernelbuf.fd(); }
			void socket(Socket&& rhs) {
				this->_stream >> underflow;
				this->_stream.clear();
				this->_kernelbuf.setfd(socket_type(std::move(rhs)));
			}
			Endpoint bind_addr() const { return this->socket().bind_addr(); }
			const Endpoint& vaddr() const { return this->_vaddr; }
			void setvaddr(const Endpoint& rhs) { this->_vaddr = rhs; }

			bool empty() const { return this->_buffer.empty(); }

			this_type* link() const { return this->_link; }
			void link(this_type* rhs) { this->_link = rhs; }

			friend std::ostream& operator<<(std::ostream& out, const this_type& rhs) {
				return out << "{vaddr="
					<< rhs.vaddr() << ",sock="
					<< rhs.socket() << ",kernels="
					<< rhs._buffer.size() << ",str="
					<< debug_stream(rhs._stream) << '}';
			}

		private:

			void read_and_send_kernel() {
				app_type app;
				this->_stream >> app;
				if (!this->_stream) return;
				if (app != Application::ROOT) {
					factory::components::forward_to_app(app, this->_kernelbuf, this->_stream);
				} else {
					Type<kernel_type>::read_object(this->_stream, [this,app] (kernel_type* k) {
						k->from(_vaddr);
						k->setapp(app);
						Logger<Level::COMPONENT>()
							<< "recv kernel=" << *k
							<< ",rdstate=" << debug_stream(this->_stream)
							<< std::endl;
						if (k->moves_downstream()) {
							this->clear_kernel_buffer(k);
						}
					}, [this] (kernel_type* k) {
						this->root()->send(k);
					});
				}
			}

			void return_kernel(kernel_type* k) {
				Logger<Level::HANDLER>()
					<< "No principal found for "
					<< *k << std::endl;
				k->principal(k->parent());
				// TODO : not safe with bare pointer
//				if (this->link()) {
//					this->link()->send(k);
//				} else {
					this->send(k);
//				}
			}

			void write_kernel(kernel_type& kernel) {
				typedef packstream::pos_type pos_type;
				pos_type old_pos = this->_stream.tellp();
				this->_stream << kernel.app();
				Type<kernel_type>::write_object(kernel, this->_stream);
				pos_type new_pos = this->_stream.tellp();
				this->_stream << end_packet;
				Logger<Level::COMPONENT>() << "send bytes="
					<< new_pos-old_pos
					<< ",stream="
					<< debug_stream(this->_stream)
					<< ",krnl=" << kernel
					<< std::endl;
			}

			void recover_kernel(kernel_type* k, server_type* root) {
				k->from(k->to());
				k->result(Result::ENDPOINT_NOT_CONNECTED);
				k->principal(k->parent());
				root->send(k);
			}

			void clear_kernel_buffer(kernel_type* k) {
				auto pos = std::find_if(_buffer.begin(), _buffer.end(), [k] (kernel_type* rhs) {
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
			
			void read_kernels() {
				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				this->simulate(Event{this->fd(), Event::In});
			}

			Endpoint _vaddr;
			Kernelbuf _kernelbuf;
			stream_type _stream;
			pool_type _buffer;

			this_type* _link;
			volatile bool _dirty = false;
		};

	}

}
