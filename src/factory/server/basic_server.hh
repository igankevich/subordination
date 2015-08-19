#ifndef FACTORY_SERVER_BASIC_SERVER_HH
#define FACTORY_SERVER_BASIC_SERVER_HH

namespace factory {

	namespace components {

		template<
			class T,
			class Kernels=std::queue<T*>,
			class Threads=std::vector<std::thread>,
			class Mutex=stdx::spin_mutex,
			class Lock=std::unique_lock<Mutex>,
			class Semaphore=unix::thread_semaphore
		>
		struct Server_with_pool: public Server<T> {

			typedef T kernel_type;
			typedef Kernels kernel_pool;
			typedef Threads thread_pool;
			typedef Mutex mutex_type;
			typedef Lock lock_type;
			typedef Semaphore sem_type;
			typedef std::vector<std::unique_ptr<kernel_type>> kernel_sack;
			typedef Server<T> base_server;
			
			Server_with_pool() = default;

			explicit
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
			send(kernel_type* kernel) override {
				lock_type lock(_mutex);
				_kernels.push(kernel);
				_semaphore.notify_one();
			}

			void
			send(kernel_type** kernels, size_t n) override {
				lock_type lock(_mutex);
				std::copy_n(kernels, n, stdx::back_inserter(_kernels));
				_semaphore.notify_one();
			}

			void
			start() override {
				base_server::start();
				std::generate(_threads.begin(), _threads.end(),
					std::bind(&Server_with_pool::new_thread, this));
			}

			void
			stop() override {
				base_server::stop();
				_semaphore.notify_all();
			}

			void
			wait() override {
				base_server::wait();
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
					[sack] (kernel_type* rhs) { rhs->mark_as_deleted(sack); });
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

			static inline std::thread
			new_thread(Server_with_pool* rhs) noexcept {
				return std::thread(std::mem_fn(&Server_with_pool::run), rhs);
			}
			
		protected:
			kernel_pool _kernels;
			thread_pool _threads;
			mutex_type _mutex;
			sem_type _semaphore;
		};

		template<class T, class Kernels=std::queue<T*>, class Threads=std::vector<std::thread>>
		using Fast_server_with_pool = Server_with_pool<T, Kernels, Threads,
			stdx::spin_mutex, stdx::simple_lock<stdx::spin_mutex>, unix::thread_semaphore>;

		template<class T, class Kernels=std::queue<T*>, class Threads=std::vector<std::thread>>
		using Standard_server_with_pool = Server_with_pool<T, Kernels, Threads,
			std::mutex, std::unique_lock<std::mutex>, std::condition_variable>;

	}

}
#endif // FACTORY_SERVER_BASIC_SERVER_HH
