namespace factory {

	namespace components {

		template<class Server, class Handler, class Kernel, class Kernel_pair, class Type>
		struct Broadcast_server: public Server_link<Broadcast_server<Server, Handler, Kernel, Kernel_pair, Type>, Server> {
			
			typedef Broadcast_server<Server, Handler, Kernel, Kernel_pair, Type> This;
			typedef typename Event_poller<Handler>::E Event;

			Broadcast_server():
				_poller(),
				_broadcast_socket(),
				_handler(nullptr),
				_cpu(0),
				_thread()
			{}

			~Broadcast_server() {}
	
			void serve() {
				_poller.run([this] (Event event) {
					std::clog << "Event " << event.user_data()->endpoint() << ' ' << event << std::endl;
					event.user_data()->handle_event(event, [this, &event] (bool overflow) {
						if (overflow) {
							_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, event.user_data()));
						} else {
							_poller.modify_socket(Event(DEFAULT_EVENTS, event.user_data()));
						}
					});
				});
			}

			void send(Kernel* kernel) {
				std::clog << "Broadcast_server::send()" << std::endl;
				if (_handler == nullptr) {
					std::stringstream msg;
					msg << "Broadcast handler is null while sending kernel. ";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				_handler->send(kernel);
				_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, _handler));
			}

			void send(Kernel_pair* kernel) {
				if (_handler == nullptr) {
					std::stringstream msg;
					msg << "Broadcast handler is null while sending kernel pair. ";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				_handler->send(kernel);
				_poller.modify_socket(Event(DEFAULT_EVENTS | EPOLLOUT, _broadcast_socket));
			}

			void start() {
				std::clog << "Broadcast_server::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}
	
			void stop_impl() {
				_poller.stop();
				std::clog << "Broadcast_server::stop_impl()" << std::endl;
			}

			void wait_impl() {
				if (_thread.joinable()) {
					_thread.join();
				}
			}

			void affinity(int cpu) { _cpu = cpu; }

			friend std::ostream& operator<<(std::ostream& out, const This* rhs) {
				return operator<<(out, *rhs);
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "bserver " << rhs._cpu;
			}
	
			void socket(Endpoint endpoint) {
				_broadcast_socket.listen(endpoint, UNRELIABLE_SOCKET);
				_handler = new Handler(_broadcast_socket, endpoint);
				_poller.register_socket(Event(DEFAULT_EVENTS, _handler));
			}

		private:
			Event_poller<Handler> _poller;
			Server_socket _broadcast_socket;
			Handler* _handler;
			int _cpu;
			std::thread _thread;

			static const int DEFAULT_EVENTS = EPOLLET | EPOLLRDHUP | EPOLLIN;
		};

		template<class Kernel, template<class X> class Pool, class Type>
		struct Broadcast_handler {

			typedef Broadcast_handler<Kernel, Pool, Type> This;

			/// Serialised kernel size cannot exceed this value,
			/// otherwise discovery messages would be fragmented.
			static const size_t MAX_MESSAGE_SIZE = 128;

			Broadcast_handler(Socket socket, Endpoint endpoint):
				_socket(socket),
				_endpoint(endpoint),
				_stream(MAX_MESSAGE_SIZE),
				_ostream(MAX_MESSAGE_SIZE),
				_mutex()
			{}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
			}

			template<class F>
			void handle_event(Event<This> event, F on_overflow) {
				if (event.is_reading()) {
					try {
						Broadcast_source src(_socket);
						_stream.fill(src);
						if (_stream.full()) {
							std::clog << "Recv " << _ostream << std::endl;
							Kernel kernel;
							kernel.read(_stream);
							kernel.from(src.from());
							kernel.act();
//							Type::types().read_and_send_object(_stream, src.from());
							_stream.reset();
						}
					} catch (Error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
					} catch (std::exception& err) {
						std::clog << String_message(err.what(), __FILE__, __LINE__, __func__);
					} catch (...) {
						std::clog << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
					}
				}
				if (event.is_writing()) {
					try {
						std::unique_lock<std::mutex> lock(_mutex);
						bool overflow = false;
						while (!overflow and !_pool.empty()) {
							if (_ostream.empty()) {
								Kernel* kernel = _pool.front();
								_pool.pop();
//								const Type* type = kernel->type();
//								if (type == nullptr) {
//									std::stringstream msg;
//									msg << "Can not find type for kernel " << kernel->id();
//									throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
//								}
//								_ostream << type->id();
								kernel->write(_ostream);
								_ostream.write_size();
								std::clog << "Send " << _ostream << std::endl;
							}
							Broadcast_sink sink(_socket, _endpoint.port());
							_ostream.flush(sink);
							if (_ostream.empty()) {
								std::clog << "Flushed." << std::endl;
								_ostream.reset();
							} else {
								overflow = true;
							}
						}
						on_overflow(overflow);
	//					std::clog << "Buffer size: " << _ostream.size() << std::endl;
	//					_socket.send(_ostream);
					} catch (Connection_error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
	//					the_server()->send(kernel);
	//					this->root()->send(kernel);
					} catch (Error& err) {
						std::clog << Error_message(err, __FILE__, __LINE__, __func__);
					} catch (std::exception& err) {
						std::clog << String_message(err, __FILE__, __LINE__, __func__);
					} catch (...) {
						std::clog << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
					}
				}
			}

			int fd() const { return _socket; }
			Socket socket() const { return _socket; }
			Endpoint endpoint() const { return _endpoint; }

		private:
			Server_socket _socket;
			Endpoint _endpoint;
			Foreign_stream _stream;
			Foreign_stream _ostream;
			std::mutex _mutex;
			Pool<Kernel*> _pool;
		};

	}

}

