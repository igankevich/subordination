namespace factory {

	namespace components {

		template<class T>
		struct CPU_server: public Server_link<CPU_server<T>, Server<T>> {

			typedef T kernel_type;
			typedef std::queue<kernel_type> kernel_pool;
			typedef std::vector<std::thread> thread_pool;
			typedef std::mutex mutex_type;
			typedef std::unique_lock<std::mutex> lock_type;
			typedef std::condition_variable sem_type;
			typedef std::vector<std::unique_ptr<kernel_type>> kernel_sack;

			CPU_server(CPU_server&& rhs) noexcept:
			_kernels(std::move(rhs._kernels)),
			_threads(std::move(rhs._threads)),
			_mutex(),
			_semaphore()
			{}

			CPU_server() noexcept:
			_kernels(),
			_threads(std::thread::hardware_concurrency()),
			_mutex(),
			_semaphore()
			{}
				
			CPU_server(const CPU_server&) = delete;
			CPU_server& operator=(const CPU_server&) = delete;

			~CPU_server() {
				// ensure that kernels inserted without starting
				// a server are deleted
				kernel_sack sack;
				collect_kernels(std::back_inserter(sack));
			}

			void
			send(kernel_type* kernel) {
				lock_type lock(_mutex);
				_kernels.push(kernel);
				_semaphore.notify_one();
			}
			
			void
			start() noexcept {
				std::generate(_threads.begin(), _threads.end(),
					[this] () { return std::move(std::thread(std::mem_fn(&CPU_server::run), this)); });
			}

			void
			stop_impl() noexcept {
				_semaphore.notify_all();
			}

			void
			wait_impl() noexcept {
				std::for_each(_threads.begin(), _threads.end(),
					[] (std::thread& rhs) {
						if (rhs.joinable()) {
							rhs.join();
						}
					}
				);
			}

			friend std::ostream&
			operator<<(std::ostream& out, const CPU_server& rhs) {
				return out << "cpuserver";
			}

		private:

			void
			run() {
				while (!this->stopped()) {
					kernel_type* kernel = recv_kernel();
					kernel->run_act();
				}
				collect_kernels();
			}

			inline kernel_type*
			recv_kernel() noexcept {
				lock_type lock(_mutex);
				_semaphore.wait(lock, std::mem_fn(&CPU_server::stopped));
				kernel_type* kernel = _kernels.front();
				_kernels.pop();
				return kernel;
			}

			template<class It>
			void
			collect_kernels(It sack) {
				stdx::front_pop_iterator<kernel_pool> it(_kernels);
				stdx::front_pop_iterator<kernel_pool> it_end;
				std::for_each(it, it_end,
					[&sack] (kernel_type* kernel) {
						kernel->mark_as_deleted(sack);
					}
				);
			}

			void
			collect_kernels() {
				// Recursively collect kernel pointers to the sack
				// and delete them all at once. Collection process
				// is fully serial to prevent multiple deletions
				// and access to unitialised values.
				std::unique_lock<std::mutex> lock(__kernel_delete_mutex);
				kernel_sack sack;
				collect_kernels(std::back_inserter(sack));
				// simple barrier for all threads participating in deletion
				global_barrier(lock);
				// destructors of scoped variables
				// will destroy all kernels automatically
			}
			
			kernel_pool _kernels;
			thread_pool _threads;
			mutex_type _mutex;
			sem_type _semaphore;
		};

	}

}
