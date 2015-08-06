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
		generate_sem_name(Process_id child, Process_id parent) {
			std::ostringstream str;
			str << "/factory-" << parent << '-' << child;
			return str.str();
		}

		template<class Server>
		struct Root_server: public Server_link<Root_server<Server>, Server> {

			typedef Root_server<Server> this_type;
			typedef typename Server::Kernel kernel_type;
			typedef Process process_type;
			typedef basic_ikernelshmembuf<char> ibuf_type;
			typedef basic_okernelshmembuf<char> obuf_type;
			typedef Packing_stream<char> stream_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;
			typedef Semaphore sem_type;
			typedef Application::id_type app_type;

			Root_server():
				_ibuf(generate_shmem_id(this_process::id(), this_process::parent_id(), 0)),
				_obuf(generate_shmem_id(this_process::id(), this_process::parent_id(), 1)),
				_istream(&this->_ibuf),
				_ostream(&this->_obuf),
				_isem(generate_sem_name(this_process::id(), this_process::parent_id())),
				_thread()
				{}

			Root_server(Root_server&& rhs):
				_ibuf(std::move(rhs._ibuf)),
				_obuf(std::move(rhs._obuf)),
				_istream(std::move(rhs._istream)),
				_ostream(std::move(rhs._ostream)),
				_isem(std::move(rhs._isem)),
				_thread(std::move(rhs._thread))
				{}

			void start() {
				this->_thread = std::thread(std::mem_fn(&this_type::serve))
			}

			void stop_impl() { this->_isem.notify_one(); }

			void wait_impl() {
				if (this->_thread.joinable()) {
					this->_thread.join();
				}
			}

			void send(kernel_type* k) {
				olock_type lock(this->_obuf);
				this->_ostream << k->app();
				Type<kernel_type>::write_object(*k, this->_ostream);
				this->_ostream << end_packet;
				this->_osem.notify_one();
			}

		private:

			void serve() {
				while (!this->stopped()) {
					this->read_and_send_kernel();
					this->_isem.wait();
				}
			}

			void read_and_send_kernel() {
				ilock_type lock(this->_ibuf);
				app_type app;
				this->_istream >> app;
				if (!this->_istream) return;
				Type<kernel_type>::read_object(this->_istream, [this,app] (kernel_type* k) {
					k->from(_vaddr);
					k->setapp(app);
					Logger<Level::COMPONENT>()
						<< "recv kernel=" << *k
						<< ",rdstate=" << debug_istream(this->_istream)
						<< std::endl;
				}, [this] (kernel_type* k) {
					this->root()->send(k);
				});
				this->_istream.clear();
			}

			ibuf_type _ibuf;
			obuf_type _obuf;
			stream_type _istream;
			stream_type _ostream;
			sem_type _isem;
			sem_type _osem;
			std::thread _thread;
		};

		template<class Server>
		struct App_Rserver: public Server_link<App_Rserver<Server>, Server> {

			typedef App_Rserver<Server> this_type;
			typedef typename Server::Kernel kernel_type;
			typedef Process process_type;
			typedef basic_ikernelshmembuf<char> ibuf_type;
			typedef basic_okernelshmembuf<char> obuf_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;

			explicit App_Rserver(process_type proc):
				_proc(proc),
				_ibuf(generate_shmem_id(this->_proc.id(), this_process::id(), 0)),
				_obuf(generate_shmem_id(this->_proc.id(), this_process::id(), 1))
				{}

			App_Rserver(App_Rserver&& rhs):
				_proc(rhs._proc),
				_ibuf(std::move(rhs._ibuf)),
				_obuf(std::move(rhs._obuf))
				{}

			process_type proc() const { return this->_proc; }

			void send(kernel_type* k) {}

			template<class X>
			void forward(basic_ikernelbuf<X>& buf, packstream& str) {
				olock_type lock(this->_obuf);
				this->_obuf.append_packet(buf);
			}

		private:
			process_type _proc;
			ibuf_type _ibuf;
			obuf_type _obuf;
		};

		template<class Server>
		struct App_Iserver: public Server_link<App_Iserver<Server>, Server> {

			typedef typename Server::Kernel Kernel;
			typedef Application app_type;
			typedef App_Rserver<Server> rserver_type;
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
				this->_apps.emplace(app.id(), rserver_type(app.execute()));
			}
			
			void send(Kernel* k) {
			}

			template<class X>
			void forward(key_type app, basic_ikernelbuf<X>& buf, packstream& str) {
				typename map_type::iterator result = this->_apps.find(app);
				if (result == this->_apps.end()) {
					throw Error("bad app id", __FILE__, __LINE__, __func__);
				}
				result->second.forward(buf, str);
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
