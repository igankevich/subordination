namespace factory {
	namespace components {

		inline
		shared_mem<char>::id_type
		generate_shmem_id(Process_id child, Process_id parent, int stream_no) {
			static const int MAX_PID = 65536;
			static const int MAX_STREAMS = 10;
			return child * MAX_PID * MAX_STREAMS + parent * MAX_STREAMS + stream_no;
		}

		inline
		std::string
		generate_sem_name(Process_id child, Process_id parent, char tag) {
			std::ostringstream str;
			str << "/factory-" << parent << '-' << child << '-' << tag;
			return str.str();
		}

		template<class Server>
		struct Principal_server: public Server_link<Principal_server<Server>, Server> {

			typedef Principal_server<Server> this_type;
			typedef typename Server::Kernel kernel_type;
			typedef Process process_type;
			typedef basic_shmembuf<char> ibuf_type;
			typedef basic_shmembuf<char> obuf_type;
			typedef Packing_stream<char> stream_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;
			typedef Semaphore sem_type;
			typedef Application::id_type app_type;

			Principal_server():
				_ibuf(),
				_obuf(),
				_istream(&this->_ibuf),
				_ostream(&this->_obuf),
				_isem(),
				_osem(),
				_thread(),
				_app(this_process::getenv("APP_ID", Application::ROOT))
				{}

			Principal_server(Principal_server&& rhs):
				_ibuf(std::move(rhs._ibuf)),
				_obuf(std::move(rhs._obuf)),
				_istream(std::move(rhs._istream)),
				_ostream(std::move(rhs._ostream)),
				_isem(std::move(rhs._isem)),
				_osem(std::move(rhs._osem)),
				_thread(std::move(rhs._thread)),
				_app(rhs._app)
				{}

			void start() {
				Logger<Level::SERVER>() << "Principal_server::start()" << std::endl;
				this->_ibuf.attach(generate_shmem_id(this_process::id(), this_process::parent_id(), 1));
				this->_obuf.attach(generate_shmem_id(this_process::id(), this_process::parent_id(), 0));
				this->_isem.open(generate_sem_name(this_process::id(), this_process::parent_id(), 'i'), false);
				this->_osem.open(generate_sem_name(this_process::id(), this_process::parent_id(), 'o'), false);
				this->_thread = std::thread(std::mem_fn(&this_type::serve), this);
			}

			void stop_impl() { this->_isem.notify_one(); }

			void wait_impl() {
				if (this->_thread.joinable()) {
					this->_thread.join();
				}
			}

			void send(kernel_type* k) {
				olock_type lock(this->_obuf);
				this->_ostream << this->_app << k->to();
				Type<kernel_type>::write_object(*k, this->_ostream);
				this->_osem.notify_one();
			}

//			void setapp(app_type app) { this->_app = app; }

		private:

			void serve() {
				while (!this->stopped()) {
					this->read_and_send_kernel();
					this->_isem.wait();
				}
			}

			void read_and_send_kernel() {
				ilock_type lock(this->_ibuf);
				this->_istream.clear();
				this->_istream.rdbuf(&this->_ibuf);
				Endpoint src;
				this->_istream >> src;
				Logger<Level::APP>() << "read_and_send_kernel(): src=" << src
					<< ",rdstate=" << debug_stream(this->_istream)
					<< std::endl;
				if (!this->_istream) return;
				Type<kernel_type>::read_object(this->_istream, [this,&src] (kernel_type* k) {
					k->from(src);
					Logger<Level::APP>()
						<< "recv kernel=" << *k
						<< ",rdstate=" << debug_stream(this->_istream)
						<< std::endl;
				}, [this] (kernel_type* k) {
					Logger<Level::APP>()
						<< "recv2 kernel=" << *k
						<< ",rdstate=" << debug_stream(this->_istream)
						<< std::endl;
					this->root()->send(k);
				});
			}

			ibuf_type _ibuf;
			obuf_type _obuf;
			stream_type _istream;
			stream_type _ostream;
			sem_type _isem;
			sem_type _osem;
			std::thread _thread;
			app_type _app;
		};

		template<class Kernel>
		struct Sub_Rserver: public Server<Kernel> {

			typedef Sub_Rserver<Kernel> this_type;
			typedef Kernel kernel_type;
			typedef Process process_type;
			typedef basic_shmembuf<char> ibuf_type;
			typedef basic_shmembuf<char> obuf_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;
			typedef Semaphore sem_type;
			typedef Packing_stream<char> stream_type;

			explicit Sub_Rserver(const Application& app):
				_proc(app.execute()), //TODO: race condition
				_osem(generate_sem_name(this->_proc.id(), this_process::id(), 'o')),
				_isem(generate_sem_name(this->_proc.id(), this_process::id(), 'i')),
				_ibuf(generate_shmem_id(this->_proc.id(), this_process::id(), 0)),
				_obuf(generate_shmem_id(this->_proc.id(), this_process::id(), 1)),
				_ostream(&this->_obuf)
				{}

			Sub_Rserver(Sub_Rserver&& rhs):
				_proc(rhs._proc),
				_osem(std::move(rhs._osem)),
				_isem(std::move(rhs._isem)),
				_ibuf(std::move(rhs._ibuf)),
				_obuf(std::move(rhs._obuf)),
				_ostream(std::move(rhs._ostream))
				{}

			process_type proc() const { return this->_proc; }

			void send(kernel_type* k) {
				olock_type lock(this->_obuf);
				stream_type os(&this->_obuf);
				// TODO: full-featured ostream is not needed here
//				this->_ostream.rdbuf(&this->_obuf);
				Logger<Level::APP>() << "write from " << k->from() << std::endl;
				os << k->from();
				Logger<Level::APP>() << "send kernel " << *k << std::endl;
				Type<kernel_type>::write_object(*k, os);
				this->_osem.notify_one();
			}

			template<class X>
			void forward(basic_ikernelbuf<X>& buf, const Endpoint& from) {
				olock_type lock(this->_obuf);
				this->_ostream.rdbuf(&this->_obuf);
				this->_ostream << from;
				append_payload(this->_obuf, buf);
				this->_osem.notify_one();
			}

		private:
			process_type _proc;
			sem_type _osem;
			sem_type _isem;
			ibuf_type _ibuf;
			obuf_type _obuf;
			stream_type _ostream;
		};

		template<class Server>
		struct Sub_Iserver: public Server {

			typedef typename Server::Kernel Kernel;
			typedef Application app_type;
			typedef Sub_Rserver<Kernel> rserver_type;
			typedef typename app_type::id_type key_type;
			typedef std::map<key_type, rserver_type> map_type;
			typedef typename map_type::value_type pair_type;

			void add(const app_type& app) {
				if (this->_apps.count(app.id()) > 0) {
					throw Error("trying to add an existing app",
						__FILE__, __LINE__, __func__);
				}
				Logger<Level::APP>() << "starting app="
					<< app << std::endl;
				std::unique_lock<std::mutex> lock(this->_mutex);
				this->_apps.emplace(app.id(), rserver_type(app));
			}
			
			void send(Kernel* k) {
				if (k->moves_everywhere()) {
//					std::for_each(this->_apps.begin(), this->_apps.end(),
//						[k] (pair_type& rhs) {
//							rhs.second.send(k);
//						}
//					);
				} else {
					typename map_type::iterator result = this->_apps.find(k->app());
					if (result == this->_apps.end()) {
						throw Error("bad app id", __FILE__, __LINE__, __func__);
					}
					result->second.send(k);
				}
			}

			template<class X>
			void forward(key_type app, const Endpoint& from, basic_ikernelbuf<X>& buf) {
				typename map_type::iterator result = this->_apps.find(app);
				if (result == this->_apps.end()) {
					throw Error("bad app id", __FILE__, __LINE__, __func__);
				}
				result->second.forward(buf, from);
			}

			void stop_impl() {
				this->_semaphore.notify_one();
			}

			void wait_impl() {
				while (!this->stopped() || !this->_apps.empty()) {
					bool empty = false;
					{
						std::unique_lock<std::mutex> lock(this->_mutex);
						this->_semaphore.wait(lock, [this] () {
							return !this->_apps.empty() || this->stopped();
						});
						empty = this->_apps.empty();
					}
					if (!empty) {
						int status = 0;
						Process_id pid = Process::wait(&status);
						std::unique_lock<std::mutex> lock(this->_mutex);
						auto result = std::find_if(this->_apps.begin(), this->_apps.end(),
							[pid] (const pair_type& rhs) {
								return rhs.second.proc().id() == pid;
							}
						);
						if (result != this->_apps.end()) {
							int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 0,
								sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
							Logger<Level::APP>() << "finished app="
								<< result->first
								<< ",ret=" << ret
								<< ",sig=" << sig
								<< std::endl;
							this->_apps.erase(result);
						} else {
							// TODO: forward pid to the thread waiting for it
						}
					}
				}
			}

		private:
			map_type _apps;
			std::mutex _mutex;
			std::condition_variable _semaphore;
		};

	}
}
