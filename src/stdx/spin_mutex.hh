#ifndef STDX_SPIN_MUTEX_HH
#define STDX_SPIN_MUTEX_HH

#include <atomic>

namespace stdx {

	/// Fast mutex for scheduling strategies.
	struct spin_mutex {

		inline void
		lock() noexcept {
			while (_flag.test_and_set(std::memory_order_acquire));
		}

		inline void
		unlock() noexcept {
			_flag.clear(std::memory_order_release);
		}

		inline bool
		try_lock() noexcept {
			return !_flag.test_and_set(std::memory_order_acquire);
		}

		inline
		spin_mutex() noexcept = default;

		// disallow copy & move operations
		spin_mutex(const spin_mutex&) = delete;
		spin_mutex(spin_mutex&&) = delete;
		spin_mutex& operator=(const spin_mutex&) = delete;
		spin_mutex& operator=(spin_mutex&&) = delete;

	private:
		std::atomic_flag _flag = ATOMIC_FLAG_INIT;
	};

	template<class Mutex>
	struct simple_lock {
		
		typedef Mutex mutex_type;

		inline
		simple_lock(mutex_type& m) noexcept:
		mtx(m)
		{
			lock();
		}

		inline
		~simple_lock() noexcept {
			unlock();
		}

		inline void
		lock() noexcept {
			mtx.lock();
		}

		inline void
		unlock() noexcept {
			mtx.unlock();
		}

		// disallow copy & move operations
		simple_lock() = delete;
		simple_lock(const simple_lock&) = delete;
		simple_lock(simple_lock&&) = delete;
		simple_lock& operator=(const simple_lock&) = delete;
		simple_lock& operator=(simple_lock&&) = delete;

	private:
		Mutex& mtx;
	};

}

#endif // STDX_SPIN_MUTEX_HH
