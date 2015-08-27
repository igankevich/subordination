#ifndef STDX_UNLOCK_GUARD_HH
#define STDX_UNLOCK_GUARD_HH

namespace stdx {

	template<class Mutex>
	struct unlock_guard {
		
		typedef Mutex mutex_type;

		inline
		unlock_guard(mutex_type& m) noexcept:
		mtx(m)
		{
			unlock();
		}

		inline
		~unlock_guard() noexcept {
			lock();
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
		unlock_guard() = delete;
		unlock_guard(const unlock_guard&) = delete;
		unlock_guard(unlock_guard&&) = delete;
		unlock_guard& operator=(const unlock_guard&) = delete;
		unlock_guard& operator=(unlock_guard&&) = delete;

	private:
		Mutex& mtx;
	};

}

#endif // STDX_UNLOCK_GUARD_HH
