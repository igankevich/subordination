#ifndef FACTORY_SERVER_CPU_SERVER_HH
#define FACTORY_SERVER_CPU_SERVER_HH

namespace factory {

	namespace components {

		template<class T, class Kernels=std::queue<T>,
		class Threads=std::vector<std::thread>, class Mutex=std::mutex,
		class Lock=std::unique_lock<Mutex>, class Semaphore=std::condition_variable>
		struct Server_with_pool: public Server<T> {

			typedef T kernel_type;
			typedef Kernels kernel_pool;
			typedef Threads thread_pool;
			typedef Mutex mutex_type;
			typedef Lock lock_type;
			typedef Semaphore sem_type;
			typedef std::vector<std::unique_ptr<kernel_type>> kernel_sack;
			typedef Server<T> basic_server;
			
			Server_with_pool() = default;

			Server_with_pool(unsigned concurrency) noexcept:
			_kernels(),
			_threads(concurrency == 0u ? 1u : concurrency),
			_mutex(),
			_semaphore()
			{}

			Server_with_pool(Server_with_pool&& rhs) noexcept:
			_kernels(std::move(rhs._kernels)),
			_threads(std::move(rhs._threads)),
			_mutex(),
			_semaphore()
			{}

			~Server_with_pool() {
				// ensure that kernels inserted without starting
				// a server are deleted
				kernel_sack sack;
				collect_kernels(std::back_inserter(sack));
			}

			Server_with_pool(const Server_with_pool&) = delete;
			Server_with_pool& operator=(const Server_with_pool&) = delete;

			void
			send(kernel_type* kernel) {
				lock_type lock(_mutex);
				_kernels.push(kernel);
				_semaphore.notify_one();
			}

			void
			start() noexcept {
				basic_server::start();
				std::generate(_threads.begin(), _threads.end(),
					std::mem_fn(&Server_with_pool::new_thread));
			}

			void
			stop() noexcept {
				basic_server::stop();
				_semaphore.notify_all();
			}

			void
			wait() noexcept {
				basic_server::wait();
				stdx::for_each_if(_threads.begin(), _threads.end(),
					std::mem_fn(&std::thread::joinable),
					std::mem_fn(&std::thread::join));
			}

		protected:
			virtual void
			do_run() = 0;

			void
			run() {
				register_server();
				do_run();
				collect_kernels();
			}

		private:

			template<class It>
			void
			collect_kernels(It sack) {
				stdx::front_pop_iterator<kernel_pool> it_end;
				std::for_each(stdx::front_popper(_kernels), it_end,
					std::bind(std::mem_fn(&kernel_type::mark_as_deleted), sack));
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

			inline std::thread&&
			new_thread() noexcept {
				return std::move(std::thread(std::mem_fn(&Server_with_pool::run), this));
			}
			
		protected:
			kernel_pool _kernels;
			thread_pool _threads;
			mutex_type _mutex;
			sem_type _semaphore;
		};

		template<class T>
		struct CPU_server: public Server_with_pool<T> {

			typedef Server_with_pool<T> base_server;
			using typename Server_with_pool<T>::kernel_type;
			using typename Server_with_pool<T>::lock_type;

			CPU_server(CPU_server&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			CPU_server() noexcept:
			base_server(std::thread::hardware_concurrency())
			{}
				
			CPU_server(const CPU_server&) = delete;
			CPU_server& operator=(const CPU_server&) = delete;
			~CPU_server() = default;

			friend std::ostream&
			operator<<(std::ostream& out, const CPU_server& rhs) {
				return out << "cpuserver";
			}

		private:

			void
			do_run() {
				while (!this->stopped()) {
					kernel_type* kernel = recv_kernel();
					kernel->run_act();
				}
			}

			inline kernel_type*
			recv_kernel() noexcept {
				lock_type lock(this->_mutex);
				this->_semaphore.wait(lock, std::mem_fn(&CPU_server::stopped));
				kernel_type* kernel = stdx::front(this->_kernels);
				stdx::pop(this->_kernels);
				return kernel;
			}
			
		};

	}

}
#endif // FACTORY_SERVER_CPU_SERVER_HH
