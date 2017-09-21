#ifndef BSCHEDULER_PPL_BASIC_PIPELINE_HH
#define BSCHEDULER_PPL_BASIC_PIPELINE_HH

#include <cassert>
#include <queue>
#include <thread>
#include <vector>

#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/ipc/thread_semaphore>
#include <unistdx/it/container_traits>
#include <unistdx/it/queue_popper>
#include <unistdx/it/queue_pusher>
#include <unistdx/util/system>

#include <bscheduler/base/thread_name.hh>
#include <bscheduler/ppl/pipeline_base.hh>
#include <bscheduler/ppl/thread_context.hh>
#include <bscheduler/kernel/kernel_type.hh>

namespace bsc {

	void
	graceful_shutdown(int ret);

	int
	wait_and_return();

	template<
		class T,
		class Kernels=std::queue<T*>,
		class Traits=sys::queue_traits<Kernels>,
		class Threads=std::vector<std::thread>,
		class Mutex=sys::spin_mutex,
		class Lock=sys::simple_lock<Mutex>,
		class Semaphore=sys::thread_semaphore
	>
	class basic_pipeline: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef Kernels kernel_pool;
		typedef Threads thread_pool;
		typedef Mutex mutex_type;
		typedef Lock lock_type;
		typedef Semaphore sem_type;
		typedef std::vector<std::unique_ptr<kernel_type> > kernel_sack;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<kernel_pool,traits_type> queue_popper;

	protected:
		kernel_pool _kernels;
		thread_pool _threads;
		mutable mutex_type _mutex;
		mutable sem_type _semaphore;

	public:
		basic_pipeline() = default;

		inline explicit
		basic_pipeline(unsigned concurrency) noexcept:
		_kernels(),
		_threads(std::max(1u, concurrency)),
		_mutex(),
		_semaphore()
		{}

		inline
		basic_pipeline(basic_pipeline&& rhs) noexcept:
		_kernels(std::move(rhs._kernels)),
		_threads(std::move(rhs._threads)),
		_mutex(),
		_semaphore()
		{}

		inline
		~basic_pipeline() {
			// ensure that kernels inserted without starting
			// a pipeline are deleted
			kernel_sack sack;
			this->collect_kernels(std::back_inserter(sack));
		}

		basic_pipeline(const basic_pipeline&) = delete;

		basic_pipeline&
		operator=(const basic_pipeline&) = delete;

		void
		send(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
			lock_type lock(this->_mutex);
			traits_type::push(this->_kernels, k);
			this->_semaphore.notify_one();
		}

		void
		send(kernel_type** kernels, size_t n) {
			lock_type lock(this->_mutex);
			#ifndef NDEBUG
			assert(
				not std::any_of(
					kernels,
					kernels+n,
					std::mem_fn(&kernel_type::moves_downstream)
				)
			);
			#endif
			std::copy_n(kernels, n, sys::queue_pusher(this->_kernels));
			this->_semaphore.notify_one();
		}

		void
		start() {
			this->setstate(pipeline_state::started);
			unsigned thread_no = this->_number;
			for (std::thread& thr : this->_threads) {
				thr = std::thread(
					[this,thread_no] () {
					    this_thread::name = this->_name;
					    this_thread::number = thread_no;
					    this->run(&this_thread::context);
					}
				      );
				++thread_no;
			}
		}

		void
		stop() {
			lock_type lock(this->_mutex);
			this->setstate(pipeline_state::stopped);
			this->_semaphore.notify_all();
		}

		void
		wait() {
			#ifndef NDEBUG
			this->log("wait(): pid=_", sys::this_process::id());
			#endif
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
#endif // vim:filetype=cpp
