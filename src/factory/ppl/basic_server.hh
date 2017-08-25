#ifndef FACTORY_BASIC_SERVER_HH
#define FACTORY_BASIC_SERVER_HH

#include <thread>
#include <queue>
#include <cassert>
#include <future>
#include <iostream>

#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/ipc/thread_semaphore>
#include <unistdx/it/container_traits>
#include <unistdx/it/queue_popper>
#include <unistdx/it/queue_pusher>
#include <unistdx/util/system>

#include <factory/reg/type.hh>
#include <factory/base/thread_name.hh>
#include "thread_context.hh"

namespace factory {

	void
	graceful_shutdown(int ret);

	int
	wait_and_return();

	enum struct server_state {
		initial,
		starting,
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

		inline void
		setstate(server_state rhs) noexcept {
			this->_state = rhs;
		}

		inline bool
		is_stopped() const noexcept {
			return this->_state == server_state::stopped;
		}

		inline bool
		is_stopping() const noexcept {
			return this->_state == server_state::stopping;
		}

		inline bool
		is_started() const noexcept {
			return this->_state == server_state::started;
		}

	protected:

		volatile server_state _state = server_state::initial;

	};

	template<
		class T,
		class Kernels=std::queue<T*>,
		class Traits=sys::queue_traits<Kernels>,
		class Threads=std::vector<std::thread>,
		class Mutex=sys::spin_mutex,
		class Lock=sys::simple_lock<Mutex>,
		class Semaphore=sys::thread_semaphore
	>
	class Basic_server: public Server_base {

	public:
		typedef T kernel_type;
		typedef Kernels kernel_pool;
		typedef Threads thread_pool;
		typedef Mutex mutex_type;
		typedef Lock lock_type;
		typedef Semaphore sem_type;
		typedef std::vector<std::unique_ptr<kernel_type>> kernel_sack;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<kernel_pool,traits_type> queue_popper;

	protected:
		kernel_pool _kernels;
		thread_pool _threads;
		mutex_type _mutex;
		sem_type _semaphore;
		const char* _name = "ppl";
		unsigned _number = 0;

	public:
		Basic_server() = default;

		inline explicit
		Basic_server(unsigned concurrency) noexcept:
		_kernels(),
		_threads(std::max(1u, concurrency)),
		_mutex(),
		_semaphore()
		{}

		inline
		Basic_server(Basic_server&& rhs) noexcept:
		_kernels(std::move(rhs._kernels)),
		_threads(std::move(rhs._threads)),
		_mutex(),
		_semaphore(),
		_name(rhs._name),
		_number(rhs._number)
		{}

		inline
		~Basic_server() {
			// ensure that kernels inserted without starting
			// a server are deleted
			kernel_sack sack;
			this->collect_kernels(std::back_inserter(sack));
		}

		Basic_server(const Basic_server&) = delete;
		Basic_server& operator=(const Basic_server&) = delete;

		void
		send(kernel_type* kernel) {
			lock_type lock(this->_mutex);
			traits_type::push(this->_kernels, kernel);
			this->_semaphore.notify_one();
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->_name = rhs;
		}

		inline void
		set_number(unsigned rhs) noexcept {
			this->_number = rhs;
		}

		void
		send(kernel_type** kernels, size_t n) {
			lock_type lock(this->_mutex);
			#ifndef NDEBUG
			assert(
				not std::any_of(
					kernels, kernels+n,
					std::mem_fn(&kernel_type::moves_downstream)
				)
			);
			#endif
			std::copy_n(kernels, n, sys::queue_pusher(this->_kernels));
			this->_semaphore.notify_one();
		}

		void
		start() {
			this->setstate(server_state::started);
			unsigned thread_no = this->_number;
			for (std::thread& thr : this->_threads) {
				thr = std::thread([this,thread_no] () {
					this_thread::name = this->_name;
					this_thread::number = thread_no;
					this->run(&this_thread::context);
				});
				++thread_no;
			}
		}

		void
		stop() {
			lock_type lock(this->_mutex);
			this->setstate(server_state::stopped);
			this->_semaphore.notify_all();
		}

		void
		wait() {
			for (std::thread& thr : this->_threads) {
				if (thr.joinable()) {
					thr.join();
				}
			}
		}

		inline unsigned
		concurrency() const noexcept {
			return this->_threads.size();
		}

	protected:

		virtual void
		do_run() = 0;

		virtual void
		run(Thread_context* context) {
			if (context) {
				Thread_context_guard lock(*context);
				context->register_thread();
			}
			this->do_run();
			if (context) {
				this->collect_kernels(*context);
			}
		}

	private:

		template<class It>
		void
		collect_kernels(It sack) {
			using namespace std::placeholders;
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[sack] (kernel_type* rhs) { rhs->mark_as_deleted(sack); }
			);
		}

		void
		collect_kernels(Thread_context& context) {
			// Recursively collect kernel pointers to the sack
			// and delete them all at once. Collection process
			// is fully serial to prevent multiple deletions
			// and access to unitialised values.
			Thread_context_guard lock(context);
			//std::clog << "global_barrier #1" << std::endl;
			context.wait(lock);
			//std::clog << "global_barrier #1 end" << std::endl;
			kernel_sack sack;
			this->collect_kernels(std::back_inserter(sack));
			// simple barrier for all threads participating in deletion
			//std::cout << "global_barrier #2" << std::endl;
			//context.global_barrier(lock);
			//std::cout << "global_barrier #2 end" << std::endl;
			// destructors of scoped variables
			// will destroy all kernels automatically
		}

	};

}
#endif // FACTORY_BASIC_SERVER_HH
