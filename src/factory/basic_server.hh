#ifndef FACTORY_BASIC_SERVER_HH
#define FACTORY_BASIC_SERVER_HH

#include <thread>
#include <queue>
#include <cassert>
#include <future>

#include <stdx/algorithm.hh>
#include <stdx/iterator.hh>
#include <stdx/mutex.hh>

#include <sys/semaphore.hh>
#include <sys/endpoint.hh>
#include <sys/system.hh>

#include <factory/type.hh>

namespace factory {

	std::promise<int> return_value;

	void
	graceful_shutdown(int ret) {
		return_value.set_value(ret);
	}

	int
	wait_and_return() {
		return return_value.get_future().get();
	}

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

	Global_thread_context _globalcon;

	enum struct server_state {
		initial,
		started,
		stopping,
		stopped
	};

	struct Server_base {

		Server_base() = default;
		virtual ~Server_base() = default;
		Server_base(Server_base&&) = default;
		Server_base(const Server_base&) = delete;
		Server_base& operator=(Server_base&) = delete;

		void
		setstate(server_state rhs) noexcept {
			_state = rhs;
		}

		bool
		is_stopped() const noexcept {
			return _state == server_state::stopped;
		}

		bool
		is_stopping() const noexcept {
			return _state == server_state::stopping;
		}

		bool
		is_started() const noexcept {
			return _state == server_state::started;
		}

	protected:

		volatile server_state _state = server_state::initial;

	};

	template<
		class T,
		class Kernels=std::queue<T*>,
		class Threads=std::vector<std::thread>,
		class Mutex=stdx::spin_mutex,
		class Lock=std::unique_lock<Mutex>,
		class Semaphore=sys::thread_semaphore
	>
	struct Server_with_pool: public Server_base {

		typedef T kernel_type;
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
		_threads(std::max(1u, concurrency)),
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
		send(kernel_type** kernels, size_t n) {
			lock_type lock(_mutex);
			#ifndef NDEBUG
			assert(
				not std::any_of(
					kernels, kernels+n,
					std::mem_fn(&kernel_type::moves_downstream)
				)
			);
			#endif
			std::copy_n(kernels, n, stdx::back_inserter(_kernels));
			_semaphore.notify_one();
		}

		void
		start() {
			setstate(server_state::started);
			std::generate(
				_threads.begin(), _threads.end(),
				std::bind(&Server_with_pool::new_thread, this)
			);
		}

		void
		stop() {
			lock_type lock(_mutex);
			setstate(server_state::stopped);
			_semaphore.notify_all();
		}

		void
		wait() {
			std::for_each(
				_threads.begin(), _threads.end(),
				[] (std::thread& rhs) {
					if (rhs.joinable()) {
						rhs.join();
					}
				}
			);
		}

	protected:

		virtual void
		do_run() = 0;

		virtual void
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
			std::for_each(
				stdx::front_popper(_kernels),
				stdx::front_popper_end(_kernels),
				[sack] (kernel_type* rhs) { rhs->mark_as_deleted(sack); }
			);
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
			//context.global_barrier(lock);
			//std::cout << "global_barrier #1 end" << std::endl;
			kernel_sack sack;
			collect_kernels(std::back_inserter(sack));
			// simple barrier for all threads participating in deletion
			//std::cout << "global_barrier #2" << std::endl;
			//context.global_barrier(lock);
			//std::cout << "global_barrier #2 end" << std::endl;
			// destructors of scoped variables
			// will destroy all kernels automatically
		}

		static inline std::thread
		new_thread(Server_with_pool* rhs) noexcept {
			return std::thread(
				&Server_with_pool::run, rhs,
				&_globalcon
			);
		}

	protected:
		kernel_pool _kernels;
		thread_pool _threads;
		mutex_type _mutex;
		sem_type _semaphore;
	};

	template<class T,
	class Kernels=std::queue<T*>,
	class Threads=std::vector<std::thread>>
	using Fast_server_with_pool = Server_with_pool<T, Kernels, Threads,
		stdx::spin_mutex, stdx::simple_lock<stdx::spin_mutex>, sys::thread_semaphore>;
		//std::mutex, std::unique_lock<std::mutex>, std::condition_variable>;

	template<class T,
	class Kernels=std::queue<T*>,
	class Threads=std::vector<std::thread>>
	using Standard_server_with_pool = Server_with_pool<T, Kernels, Threads,
		std::mutex, std::unique_lock<std::mutex>, std::condition_variable>;

	struct Dummy_server {
		template<class T>
		void send(T*) {}
	};

}
#endif // FACTORY_BASIC_SERVER_HH
