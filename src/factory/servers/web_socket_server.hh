namespace factory {

	namespace components {

		template<class Server, class Type>
		class Web_socket_server: public Server {
		public:
			typedef typename Server::Kernel Kernel;

			explicit Web_socket_server(ws_ctx_t* context): _web_socket(context) {}

			~Web_socket_server() {
				std::clog << "STOPPED = " << std::boolalpha << this->stopped() << std::endl;
			}

			void send(Kernel* kernel) {
				try {
					const Type* type = kernel->type();
					if (type == nullptr) {
						std::stringstream msg;
						msg << "Can not find type for kernel " << kernel->id();
						throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
					}
					std::clog << "Writing to buffer " << std::endl;
					Foreign_stream packet;
					packet << type->id();
					kernel->write(packet);
					packet.flush(_web_socket);
//					std::clog << "Buffer size: " << packet.size() << std::endl;
					std::clog << "Sent buffer: " << packet << std::endl;
				} catch (Connection_error& err) {
					std::clog << Error_message(err, __FILE__, __LINE__, __func__);
					this->root()->send(kernel);
				} catch (Error& err) {
					std::clog << Error_message(err, __FILE__, __LINE__, __func__);
				} catch (std::exception& err) {
					std::clog << String_message(err, __FILE__, __LINE__, __func__);
				} catch (...) {
					std::clog << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__);
				}
			}

			void* profiler() { return this; }
			void wait_impl() { }
			void stop_impl() { }
		
			void write(std::ostream& out) const {
				out << "socket_rserver " << _web_socket;
			}

		private:
			Web_socket _web_socket;
		};

	}

}
