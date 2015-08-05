namespace factory {
	namespace components {

		struct Application {

			typedef uint16_t id_type;
			typedef std::string path_type;

			explicit Application(const path_type& exec, id_type id):
				_execpath(exec), _id(id) {}

			Process execute() const {
				return Process([this] () {
					return this_process::execute(this->_execpath);
				});
			}

			bool operator<(const Application& rhs) const {
				return this->_id < rhs._id;
			}

			friend std::ostream& operator<<(std::ostream& out, const Application& rhs) {
				return out
					<< "{exec=" << rhs._execpath
					<< ",id=" << rhs._id << '}';
			}

		private:
			path_type _execpath;
			id_type _id;
		};

		template<class Server>
		struct App_Rserver: public Server_link<App_Rserver<Server>, Server> {

			typedef App_Rserver<Server> this_type;
			typedef typename Server::Kernel kernel_type;
			typedef Process process_type;
			typedef Application app_type;
			
			explicit App_Rserver(const app_type& app):
				_app(app), _proc(app.execute()) {}

			process_type proc() const { return this->_proc; }

			void send(kernel_type* k) {
			}

		private:
			app_type _app;
			process_type _proc;
		};

		template<class Server>
		struct App_Iserver: public Server_link<App_Iserver<Server>, Server> {

			typedef typename Server::Kernel Kernel;
			typedef App_Rserver<Server> rserver_type;
			typedef typename rserver_type::app_type app_type;
			typedef std::map<app_type, rserver_type> map_type;
			typedef typename map_type::value_type pair_type;

			void add(const app_type& app) {
				Logger<Level::APP>() << "starting app="
					<< app << std::endl;
				std::unique_lock<std::mutex> lock(this->_mutex);
				this->_apps[app] = rserver_type(app);
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
						int pid = check_if_not<EINTR>(::wait(&status),
							__FILE__, __LINE__, __func__);
						std::unique_lock<std::mutex> lock(this->_mutex);
						auto result = std::find_if(this->_apps.begin(), this->_apps.end(),
							[pid] (const pair_type& rhs) {
								return rhs.second.proc().id() == pid;
							}
						);
						if (result != this->_apps.end()) {
							Logger<Level::APP>() << "finished app="
								<< result->first << std::endl;
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
