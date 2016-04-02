#ifndef SYSX_SEMAPHORE_HH
#define SYSX_SEMAPHORE_HH

#include <unistd.h>
#include <signal.h>

// TODO sysv semaphores testing
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if _POSIX_SEMAPHORES > 0 || defined(__MACH__)
	#include <semaphore.h>
#elif _XOPEN_SHM > 0
	#include <sys/types.h>
	#include <sys/ipc.h>
	#include <sys/sem.h>
	#error implement SysV semaphores
	// TODO: implement SysV semaphores
#else
	#error can not find POSIX/SysV semaphores API
#endif

#include <condition_variable>
#include <cassert>

#include <stdx/mutex.hh>

#include <sysx/fildes.hh>
#include <sysx/process.hh>

namespace sysx {

	struct sysv_semaphore {

		typedef int sem_type;
		typedef struct ::sembuf sembuf_type;

		explicit
		sysv_semaphore(mode_type mode=0600): _owner(true) {
			_sem = bits::check(::semget(IPC_PRIVATE, _nsems, IPC_CREAT|mode),
				__FILE__, __LINE__, __func__);
		}

		explicit
		sysv_semaphore(sem_type sem):
		_sem(sem), _owner(false)
		{}

		~sysv_semaphore() {
			if (_owner) {
				bits::check(::semctl(_sem, IPC_RMID, 0),
					__FILE__, __LINE__, __func__);
			}
		}

		sem_type id() const { return _sem; }

		void wait() { increment(-1); }
		void notify_one() { increment(1); }
		void notify_all() { increment(1000); }

		template<class Lock>
		void wait(Lock& lock) {
			stdx::unlock_guard<Lock> unlock(lock);
			wait();
		}

		template<class Lock, class Pred>
		void wait(Lock& lock, Pred pred) {
			while (!pred()) {
				wait(lock);
			}
		}

	private:

		void
		increment(int how) {
			sembuf_type buf;
			buf.sem_num = 0;
			buf.sem_op = how;
			buf.sem_flg = SEM_UNDO;
			bits::check(::semop(_sem, &buf, _nsems),
				__FILE__, __LINE__, __func__);
		}

		sem_type _sem;
		bool _owner;

		static const int _nsems = 1;
	};

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
			rhs._sem = SEM_FAILED;
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
						this->_path.c_str(), __LINE__, __func__);
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
			stdx::unlock_guard<Lock> unlock(lock);
			wait();
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
			stdx::unlock_guard<Lock> unlock(lock);
			bits::check_if_not<std::errc::timed_out>(::sem_timedwait(&_sem, &timeout),
				__FILE__, __LINE__, __func__);
			std::cv_status st = std::errc(errno) == std::errc::timed_out
				? std::cv_status::timeout : std::cv_status::no_timeout;
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

		void
		destroy() {
			bits::check(::sem_destroy(&_sem),
				__FILE__, __LINE__, __func__);
		}

		void
		init_sem() {
			bits::check(::sem_init(&_sem, 0, 0),
				__FILE__, __LINE__, __func__);
		}

		sem_type _sem;
	};
	#endif

	struct sigset_type: public ::sigset_t {

		sigset_type() {
			bits::check(::sigemptyset(this),
				__FILE__, __LINE__, __func__);
		}

		explicit
		sigset_type(signal_type s):
		sigset_type()
		{
			bits::check(::sigaddset(this, s),
				__FILE__, __LINE__, __func__);
		}

		template<class F>
		void for_each(F func) {
			for (signal_type s=1; s<=31; ++s) {
				if (bits::check(::sigismember(this, s),
					__FILE__, __LINE__, __func__))
				{
					func(s);
				}
			}
		}

	};

	struct signal_lock {

		explicit
		signal_lock(sigset_type s):
		_signals(s)
		{ lock(); }

		~signal_lock() {
			unlock();
		}

		void lock() {
			change_mask(SIG_BLOCK);
		}

		void unlock() {
			change_mask(SIG_UNBLOCK);
		}

	private:

		void change_mask(int how) {
			bits::check(::sigprocmask(how, &_signals, 0),
				__FILE__, __LINE__, __func__);
		}

		sigset_type _signals;
	};

	struct signal_semaphore {

		typedef siginfo_t siginfo_type;
		typedef union ::sigval payload_type;

		explicit
		signal_semaphore(pid_type owner, signal_type s):
		_owner(owner), _sig(s), _lastnotifier(0)
		{}

		void
		notify_one() {
			// notifying non-existent process is not an error
			bits::check_if_not<std::errc::no_such_process>(
				::kill(_owner, _sig),
				__FILE__, __LINE__, __func__);
		}

		void
		notify_one(payload_type payload) {
			bits::check_if_not<std::errc::no_such_process>(
				::sigqueue(_owner, _sig, payload),
				__FILE__, __LINE__, __func__);
		}

		void
		notify_all() {
			notify_one();
		}

		void wait() {
			sigset_type sigs(_sig);
			signal_lock lock(sigs);
			siginfo_type result;
			std::cerr << "Waiting for signal sig=" << _sig << std::endl;
			bits::check_if_not<std::errc::interrupted>(
				::sigwaitinfo(&sigs, &result),
				__FILE__, __LINE__, __func__);
			if (std::errc(errno) != std::errc::interrupted) {
				_lastnotifier = result.si_pid;
				if (result.si_code == SI_QUEUE) {
					std::clog << "Received sival=" << result.si_value.sival_int << std::endl;
					_queue.push(result.si_value);
				}
			}
		}

		template<class Lock>
		void wait(Lock& lock) {
			stdx::unlock_guard<Lock> unlock(lock);
			wait();
		}

		template<class Lock, class Pred>
		void wait(Lock& lock, Pred pred) {
			while (!pred()) {
				wait(lock);
			}
		}

		pid_type
		last_notifier() {
			return _lastnotifier;
		}

		const std::queue<payload_type>&
		payload_queue() const {
			return _queue;
		}

		std::queue<payload_type>&
		payload_queue() {
			return _queue;
		}

	private:

		pid_type _owner;
		signal_type _sig;
		pid_type _lastnotifier;
		std::queue<payload_type> _queue;
	};

	struct init_signal_semaphore {

		/// Block signals from set @s in all threads
		/// (this object should be constructed
		/// before launching any threads, so
		/// that they inherit sigmask from
		/// the main thread).
		explicit
		init_signal_semaphore(sigset_type s):
		_lock(s)
		{
			install_empty_signal_handler(s);
		}

		explicit
		init_signal_semaphore(signal_type s):
		init_signal_semaphore(sigset_type(s))
		{}

	private:

		/// install empty signal handler
		/// to make signal queueable,
		/// otherwise it kills receiver process
		void
		install_empty_signal_handler(sigset_type s) {
			signal_action action{};
			action.sa_handler = &init_signal_semaphore::no_handler;
			action.sa_flags = SA_SIGINFO;
			action.sa_mask = sigset_type{};
			s.for_each([this,&action](signal_type sig) {
				this_process::bind_signal(signal(sig), action);
			});
		}

		static void no_handler(int) {}

		/// block/unblock signal on construction/destruction
		signal_lock _lock;
	};

}

#endif // SYSX_SEMAPHORE_HH
