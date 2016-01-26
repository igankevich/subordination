#ifndef FACTORY_SERVER_BASIC_SERVER_HH
#define FACTORY_SERVER_BASIC_SERVER_HH

#include <thread>
#include <queue>

#include <factory/managed_object.hh>
#include <factory/type.hh>
#include <sysx/semaphore.hh>
#include <sysx/endpoint.hh>
#include <stdx/for_each.hh>
#include <stdx/back_inserter.hh>
#include <stdx/front_popper.hh>

namespace factory {

	namespace components {

		constexpr const Id SHUTDOWN_ID = 123;

		struct Global_thread_context {

			typedef std::mutex slowmutex_type;
			typedef std::unique_lock<slowmutex_type> slowlock_type;
			typedef stdx::spin_mutex fastmutex_type;

			void
			register_thread() noexcept {
				slowlock_type lock(_slowmutex);
				++_nthreads;
				++_saved_nthreads;
			}

			void
			global_barrier(slowlock_type& lock) noexcept {
				if (--_nthreads == 0) {
					_nthreads = _saved_nthreads;
					_semaphore.notify_all();
				} else {
					_semaphore.wait(lock);
				}
			}

			slowmutex_type&
			slowmutex() noexcept {
				return _slowmutex;
			}

		private:
			fastmutex_type _fastmutex;
			slowmutex_type _slowmutex;
			int32_t _nthreads = 0;
			int32_t _saved_nthreads = 0;
			std::condition_variable _semaphore;
		};

		enum struct server_state {
			initial,
			started,
			stopping,
			stopped
		};

		struct Resident {

			virtual void
			start() {
				setstate(server_state::started);
			}

			virtual void
			shutdown() {
				setstate(server_state::stopping);
			}

			virtual void
			stop() {
				setstate(server_state::stopped);
			}

			virtual void
			wait() {}

			// TODO: boilerplate :(
			virtual sysx::endpoint
			addr() const {
				return sysx::endpoint();
			}

			void
			setstate(server_state rhs) noexcept {
				_state = rhs;
			}

			bool
			stopped() const noexcept {
				return _state == server_state::stopped;
			}

			bool
			stopping() const noexcept {
				return _state == server_state::stopping;
			}

			bool
			started() const noexcept {
				return _state == server_state::started;
			}

			void
			set_global_context(Global_thread_context* rhs) noexcept {
				_globalcon = rhs;
			}

			Global_thread_context*
			global_context() noexcept {
				return _globalcon;
			}

		private:
			volatile server_state _state = server_state::initial;
			Global_thread_context* _globalcon = nullptr;
		};

		template<class Config>
		struct Server: public Resident {

			typedef typename Config::kernel kernel_type;
			typedef typename Config::factory factory_type;
			typedef typename Config::local_server Local_server;
			typedef typename Config::remote_server Remote_server;
			typedef typename Config::external_server External_server;
			typedef typename Config::timer_server Timer_server;
			typedef typename Config::app_server App_server;
			typedef typename Config::principal_server Principal_server;

			Server() = default;
			~Server() = default;
			Server(Server&&) = default;
			Server(const Server&) = delete;
			Server& operator=(Server&) = delete;

			virtual void send(kernel_type*) = 0;
			virtual void send(kernel_type**, size_t) {}

			factory_type*
			factory() noexcept {
				return _root;
			}

			void
			setfactory(factory_type* rhs) noexcept {
				_root = rhs;
			}

			void
			setparent(Server* rhs) noexcept {
				if (rhs) {
					setfactory(rhs->_root);
				}
			}

			Local_server* local_server() { return _root->local_server(); }
			Remote_server* remote_server() { return _root->remote_server(); }
			External_server* ext_server() { return _root->ext_server(); }
			Timer_server* timer_server() { return _root->timer_server(); }
			App_server* app_server() { return _root->app_server(); }
			Principal_server* principal_server() { return _root->principal_server(); }

		protected:
			factory_type* _root = nullptr;
		};

		template<
			class T,
			class Kernels=std::queue<typename Server<T>::kernel_type*>,
			class Threads=std::vector<std::thread>,
			class Mutex=stdx::spin_mutex,
			class Lock=std::unique_lock<Mutex>,
			class Semaphore=sysx::thread_semaphore
		>
		struct Server_with_pool: public Managed_object<Server<T>> {

			typedef Server<T> base_server;
			using typename base_server::kernel_type;
			typedef Kernels kernel_pool;
			typedef Threads thread_pool;
			typedef Mutex mutex_type;
			typedef Lock lock_type;
			typedef Semaphore sem_type;
			typedef std::vector<std::unique_ptr<kernel_type>> kernel_sack;

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
				lock_type lock(_mutex);
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
			run(Global_thread_context* context) {
				if (context) {
					context->register_thread();
				}
				do_run();
				if (context) {
					collect_kernels(*context);
				}
			}

		private:

			template<class It>
			void
			collect_kernels(It sack) {
				using namespace std::placeholders;
				stdx::front_pop_iterator<kernel_pool> it_end;
				std::for_each(stdx::front_popper(_kernels), it_end,
					[sack] (kernel_type* rhs) { rhs->mark_as_deleted(sack); });
			}

			void
			collect_kernels(Global_thread_context& context) {
				// Recursively collect kernel pointers to the sack
				// and delete them all at once. Collection process
				// is fully serial to prevent multiple deletions
				// and access to unitialised values.
				typedef Global_thread_context::slowlock_type lock_type;
				lock_type lock(context.slowmutex());
				//std::cout << "global_barrier #1" << std::endl;
				context.global_barrier(lock);
				//std::cout << "global_barrier #1 end" << std::endl;
				kernel_sack sack;
				collect_kernels(std::back_inserter(sack));
				// simple barrier for all threads participating in deletion
				//std::cout << "global_barrier #2" << std::endl;
				context.global_barrier(lock);
				//std::cout << "global_barrier #2 end" << std::endl;
				// destructors of scoped variables
				// will destroy all kernels automatically
			}

			static inline std::thread
			new_thread(Server_with_pool* rhs) noexcept {
				return std::thread(&Server_with_pool::run, rhs,
					rhs->global_context());
			}

		protected:
			kernel_pool _kernels;
			thread_pool _threads;
			mutex_type _mutex;
			sem_type _semaphore;
		};

		template<class T,
		class Kernels=std::queue<typename Server<T>::kernel_type*>,
		class Threads=std::vector<std::thread>>
		using Fast_server_with_pool = Server_with_pool<T, Kernels, Threads,
			stdx::spin_mutex, stdx::simple_lock<stdx::spin_mutex>, sysx::thread_semaphore>;

		template<class T,
		class Kernels=std::queue<typename Server<T>::kernel_type*>,
		class Threads=std::vector<std::thread>>
		using Standard_server_with_pool = Server_with_pool<T, Kernels, Threads,
			std::mutex, std::unique_lock<std::mutex>, std::condition_variable>;

		template<class Unused>
		struct No_server: public Managed_object<Server<Unused>> {
			using typename Server<Unused>::kernel_type;
			void send(kernel_type*) override {}
			template<class ... Args>
			void forward(Args&& ...) {}
			void
			setparent(Managed_object<Server<Unused>>*) override {}
			Category
			category() const noexcept override {
				return Category{
					"no_server",
					[] () { return new No_server; }
				};
			}
		};
	}

}
#endif // FACTORY_SERVER_BASIC_SERVER_HH
