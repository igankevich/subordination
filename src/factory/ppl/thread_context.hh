#ifndef FACTORY_PPL_THREAD_CONTEXT_HH
#define FACTORY_PPL_THREAD_CONTEXT_HH

#include <mutex>
#include <unistdx/base/simple_lock>
#include <unistdx/ipc/thread_semaphore>

namespace factory {

	struct Thread_context {

		typedef std::mutex mutex_type;
		typedef sys::thread_semaphore sem_type;

		inline void
		register_thread() noexcept {
			++this->_nthreads;
			++this->_saved_nthreads;
		}

		inline void
		lock() {
			this->_mtx.lock();
		}

		inline void
		unlock() {
			this->_mtx.unlock();
		}

		template <class Lock>
		void
		wait(Lock& lock) {
			if (--this->_nthreads == 0) {
				this->_nthreads = this->_saved_nthreads;
				this->_semaphore.notify_all();
			} else {
				this->_semaphore.wait(lock);
			}
		}

	private:
		volatile int _nthreads = 0;
		volatile int _saved_nthreads = 0;
		mutex_type _mtx;
		sem_type _semaphore;
	};

	typedef sys::simple_lock<Thread_context> Thread_context_guard;

	namespace this_thread {

		extern Thread_context context;

	}

}

#endif // vim:filetype=cpp
