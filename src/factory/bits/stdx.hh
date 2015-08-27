#ifndef FACTORY_BITS_STDX_HH
#define FACTORY_BITS_STDX_HH

namespace factory {

	namespace bits {

		template<class Mutex>
		struct no_lock {
			
			constexpr no_lock(Mutex& m) noexcept {}
			~no_lock() noexcept = default;
			inline void lock() noexcept {}
			inline void unlock() noexcept {}

			// disallow copy & move operations
			no_lock() = delete;
			no_lock(const no_lock&) = delete;
			no_lock(no_lock&&) = delete;
			no_lock& operator=(const no_lock&) = delete;
			no_lock& operator=(no_lock&&) = delete;

		};

		struct no_mutex {
			
			constexpr no_mutex() noexcept = default;
			~no_mutex() noexcept = default;
			inline void lock() noexcept {}
			inline bool try_lock() noexcept { return true; }
			inline void unlock() noexcept {}

			// disallow copy & move operations
			no_mutex(const no_mutex&) = delete;
			no_mutex(no_mutex&&) = delete;
			no_mutex& operator=(const no_mutex&) = delete;
			no_mutex& operator=(no_mutex&&) = delete;

		};

		struct no_semaphore {

			constexpr no_semaphore() noexcept = default;
			~no_semaphore() = default;

			template<class Lock>
			inline void wait(Lock&) {}

			template<class Lock, class Pred>
			inline void wait(Lock&, Pred) {}

			template<class Lock, class Rep>
			inline std::cv_status
			wait_for(Lock&, const Rep&) {
				return std::cv_status::no_timeout;
			}

			template<class Lock, class Duration, class Pred>
			inline bool
			wait_for(Lock&, const Duration&, Pred) {
				return true;
			}

			template<class Lock, class Duration>
			inline std::cv_status
			wait_until(Lock&, const Duration&) {
				return std::cv_status::no_timeout;
			}

			template<class Lock, class Clock, class Duration, class Pred>
			inline bool
			wait_until(Lock&, const Duration&, Pred) {
				return true;
			}

			inline void notify_one() noexcept {}
			inline void notify_all() noexcept {}

		};

		static_assert(std::is_empty<no_lock<int>>::value, "bad no_lock type");
		static_assert(std::is_empty<no_mutex>::value, "bad no_mutex type");
		static_assert(std::is_empty<no_semaphore>::value, "bad no_semaphore type");
	
	}

}

#endif // FACTORY_BITS_STDX_HH
