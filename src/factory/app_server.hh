namespace factory {
	namespace components {

		template<class Server>
		struct App_Rserver: public Server_link<App_Rserver<Server>, Server> {

			typedef App_Rserver<Server> this_type;
			typedef typename Server::Kernel kernel_type;
			typedef Process process_type;
			typedef basic_ikernelshmembuf<char> ibuf_type;
			typedef basic_okernelshmembuf<char> obuf_type;
			typedef typename ibuf_type::id_type buf_id_type;

			explicit App_Rserver(Process proc):
				_proc(proc),
				_ibuf(generate_id_for_parent(0)),
				_obuf(generate_id_for_parent(1))
				{}

			App_Rserver(App_Rserver&& rhs):
				_proc(rhs._proc),
				_ibuf(std::move(rhs._ibuf)),
				_obuf(std::move(rhs._obuf))
				{}

			process_type proc() const { return this->_proc; }

			void send(kernel_type* k) {}

		private:

			buf_id_type generate_id_for_parent(int c) const {
				return this->_proc.id() * 65536 * 2
					+ this_process::id() * 2 + c;
			}

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
