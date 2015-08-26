#ifndef FACTORY_UNISTDX_SEMAPHORE_HH
#define FACTORY_UNISTDX_SEMAPHORE_HH

#if _POSIX_SEMAPHORES > 0 || defined(__MACH__)
	#include <semaphore.h>
#else
	#include <sys/types.h>
	#include <sys/ipc.h>
	#include <sys/sem.h>
	// TODO: implement SysV semaphores
#endif

#include "../bits/check.hh"

namespace factory {

	namespace unix {

		struct process_semaphore {

			typedef ::sem_t* sem_type;
			typedef int flags_type;
			typedef std::string path_type;

			process_semaphore() = default;

			explicit
			process_semaphore(path_type&& name, mode_type mode):
				_path(std::forward<path_type>(name)),
				_owner(true),
				_sem(this->open_sem(mode))
				{}

			explicit
			process_semaphore(path_type&& name):
				_path(std::forward<path_type>(name)),
				_owner(false),
				_sem(this->open_sem())
				{}

			process_semaphore(process_semaphore&& rhs) noexcept:
				_path(std::move(rhs._path)),
				_owner(rhs._owner),
				_sem(rhs._sem)
			{
				rhs._sem = nullptr;
			}

			~process_semaphore() {
				this->close();
			}
		
			process_semaphore(const process_semaphore&) = delete;
			process_semaphore& operator=(const process_semaphore&) = delete;

			void
			open(path_type&& name, mode_type mode) {
				this->close();
				this->_path = std::forward<path_type>(name),
				this->_owner = true;
				this->_sem = this->open_sem(mode);
			}

			void
			open(path_type&& name) {
				this->close();
				this->_path = std::forward<path_type>(name),
				this->_owner = false;
				this->_sem = this->open_sem();
			}

			void
			wait() {
				bits::check(::sem_wait(this->_sem),
					__FILE__, __LINE__, __func__);
			}

			void
			notify_one() {
				bits::check(::sem_post(this->_sem),
					__FILE__, __LINE__, __func__);
			}

			void
			notify_all() {
				for (int i=0; i<1000; ++i) {
					notify_one();
				}
			}

			void
			close() {
				if (this->_sem != SEM_FAILED) {
					bits::check(::sem_close(this->_sem),
						__FILE__, __LINE__, __func__);
					if (this->_owner) {
						bits::check(::sem_unlink(this->_path.c_str()),
							__FILE__, __LINE__, __func__);
					}
				}
			}
		
		private:

			sem_type
			open_sem(mode_type mode) const {
				return bits::check(::sem_open(this->_path.c_str(),
					O_CREAT | O_EXCL, mode, 0), SEM_FAILED,
					__FILE__, __LINE__, __func__);
			}

			sem_type
			open_sem() const {
				return bits::check(::sem_open(this->_path.c_str(), 0),
					SEM_FAILED, __FILE__, __LINE__, __func__);
			}

			path_type _path;
			bool _owner = false;
			sem_type _sem = SEM_FAILED;
		};

		#if defined(__MACH__)
		typedef std::condition_variable_any thread_semaphore;
		#else
		struct thread_semaphore {

			typedef ::sem_t sem_type;
			typedef std::chrono::system_clock clock_type;
			typedef struct ::timespec timespec_type;

			thread_semaphore():
			_sem()
			{
				init_sem();
			}

			~thread_semaphore() {
				destroy();
			}

			void wait() {
				bits::check(::sem_wait(&_sem),
					__FILE__, __LINE__, __func__);
			}

			template<class Lock>
			void wait(Lock& lock) {
				lock.unlock();
				wait();
				lock.lock();
			}

			template<class Lock, class Pred>
			void wait(Lock& lock, Pred pred) {
				while (!pred()) {
					wait(lock);
				}
			}

			template<class Lock, class Rep, class Period>
			std::cv_status
			wait_for(Lock& lock, const std::chrono::duration<Rep,Period>& dur) {
				return wait_until(lock, clock_type::now() + dur);
			}

			template<class Lock, class Rep, class Period, class Pred>
			bool
			wait_for(Lock& lock, const std::chrono::duration<Rep,Period>& dur, Pred pred) {
				return wait_until(lock, clock_type::now() + dur, pred);
			}

			template<class Lock, class Duration>
			std::cv_status
			wait_until(Lock& lock, const std::chrono::time_point<clock_type,Duration>& tp) {
				using namespace std::chrono;
				const auto s = time_point_cast<seconds>(tp);
				const auto ns = duration_cast<nanoseconds>(tp - s);
				const timespec_type timeout{s.time_since_epoch().count(), ns.count()};
				lock.unlock();
				#if !defined(__MACH__)
				bits::check_if_not<std::errc::timed_out>(::sem_timedwait(&_sem, &timeout),
					__FILE__, __LINE__, __func__);
				#endif
				std::cv_status st = std::errc(errno) == std::errc::timed_out
					? std::cv_status::timeout : std::cv_status::no_timeout;
				lock.lock();
				return st;
			}

			template<class Lock, class Duration, class Pred>
			bool
			wait_until(Lock& lock, const std::chrono::time_point<clock_type,Duration>& tp, Pred pred) {
				while (!pred()) {
					if (wait_until(lock, tp) == std::cv_status::timeout) {
						return pred();
					}
				}
				return true;
			}

			template<class Lock, class Clock, class Duration>
			std::cv_status
			wait_until(Lock& lock, const std::chrono::time_point<Clock,Duration>& tp) {
				typedef Clock other_clock;
				const auto delta = tp - other_clock::now();
				const auto new_tp = clock_type::now() + delta;
				return wait_until(lock, new_tp);
			}

			template<class Lock, class Clock, class Duration, class Pred>
			bool
			wait_until(Lock& lock, const std::chrono::time_point<Clock,Duration>& tp, Pred pred) {
				typedef Clock other_clock;
				const auto delta = tp - other_clock::now();
				const auto new_tp = clock_type::now() + delta;
				return wait_until(lock, new_tp, pred);
			}

			void
			notify_one() {
				bits::check(::sem_post(&_sem),
					__FILE__, __LINE__, __func__);
			}

			void
			notify_all() {
				for (int i=0; i<1000; ++i) {
					notify_one();
				}
			}

		private:

			inline void
			destroy() {
				bits::check(::sem_destroy(&_sem),
					__FILE__, __LINE__, __func__);
			}

			inline void
			init_sem() {
				bits::check(::sem_init(&_sem, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			sem_type _sem;
		};
		#endif

	}

}

#endif // FACTORY_UNISTDX_SEMAPHORE_HH
