#ifndef SYSX_PROCESS_HH
#define SYSX_PROCESS_HH

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

#include <algorithm>
#include <vector>

#include <stdx/log.hh>
#include <stdx/intersperse_iterator.hh>

#include <sysx/bits/check.hh>
#include <sysx/bits/to_string.hh>
#include <sysx/bits/safe_calls.hh>

namespace sysx {

	typedef ::pid_t pid_type;
	typedef int stat_type;
	typedef int code_type;
	typedef ::siginfo_t siginfo_type;
	typedef struct ::sigaction sigaction_type;
	typedef int signal_type;

	struct Action: public sigaction_type {
		Action() {}
		Action(void (*func)(int)) noexcept {
			this->sa_handler = func;
		}
	};

	pid_type
	safe_fork() {
		bits::global_lock_type lock(bits::__forkmutex);
		return ::fork();
	}

	enum wait_flags {
		proc_exited = WEXITED,
		proc_stopped = WSTOPPED,
		proc_continued = WCONTINUED
	};

	template<class T>
	struct basic_status {};

	template<>
	struct basic_status<stat_type> {

		explicit constexpr
		basic_status(stat_type rhs) noexcept:
			stat(rhs) {}

		constexpr
		basic_status() noexcept = default;

		constexpr
		basic_status(const basic_status&) noexcept = default;

		constexpr bool
		exited() const noexcept {
			return WIFEXITED(stat);
		}

		constexpr bool
		killed() const noexcept {
			return WIFSIGNALED(stat);
		}

		constexpr bool
		stopped() const noexcept {
			return WIFSTOPPED(stat);
		}

		constexpr bool
		core_dumped() const noexcept {
			#ifdef WCOREDUMP
			return static_cast<bool>(WCOREDUMP(stat));
			#else
			return false;
			#endif
		}

		constexpr bool
		trapped() const noexcept {
			return false;
		}

		constexpr bool
		continued() const noexcept {
			return WIFCONTINUED(stat);
		}

		constexpr code_type
		exit_code() const noexcept {
			return WEXITSTATUS(stat);
		}

		constexpr code_type
		term_signal() const noexcept {
			return WTERMSIG(stat);
		}

		constexpr code_type
		stop_signal() const noexcept {
			return WSTOPSIG(stat);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const basic_status& rhs) {
			if (rhs.exited()) {
				out << stdx::make_fields("status", "exited", "exit_code", rhs.exit_code());
			}
			if (rhs.killed()) {
				out << stdx::make_fields("status", "killed", "term_signal", rhs.term_signal());
			}
			if (rhs.stopped()) {
				out << stdx::make_fields("status", "stopped", "stop_signal", rhs.stop_signal());
			}
			if (rhs.continued()) {
				out << stdx::make_fields("status", "continued");
			}
			return out;
		}

		stat_type stat = 0;
	};

	template<>
	struct basic_status<siginfo_type> {

		enum struct st {
			exited = CLD_EXITED,
			killed = CLD_KILLED,
			core_dumped = CLD_DUMPED,
			stopped = CLD_STOPPED,
			trapped = CLD_TRAPPED,
			continued = CLD_CONTINUED
		};

		explicit constexpr
		basic_status(const siginfo_type& rhs) noexcept:
			stat(static_cast<st>(rhs.si_code)),
			code(rhs.si_status),
			_pid(rhs.si_pid) {}

		constexpr
		basic_status() noexcept = default;

		constexpr
		basic_status(const basic_status&) noexcept = default;

		constexpr bool
		exited() const noexcept {
			return stat == st::exited;
		}

		constexpr bool
		signaled() const noexcept {
			return stat == st::killed;
		}

		constexpr bool
		stopped() const noexcept {
			return stat == st::stopped;
		}

		constexpr bool
		core_dumped() const noexcept {
			return stat == st::core_dumped;
		}

		constexpr bool
		trapped() const noexcept {
			return stat == st::trapped;
		}

		constexpr bool
		continued() const noexcept {
			return stat == st::continued;
		}

		constexpr code_type
		exit_code() const noexcept {
			return code;
		}

		constexpr code_type
		term_signal() const noexcept {
			return code;
		}

		constexpr code_type
		stop_signal() const noexcept {
			return code;
		}

		constexpr pid_type
		pid() const noexcept {
			return _pid;
		}

		st stat = static_cast<st>(0);
		code_type code = 0;
		pid_type _pid = 0;
	};

	typedef basic_status<stat_type> proc_status;
	typedef basic_status<siginfo_type> proc_info;

	struct process {

		inline
		process() noexcept = default;

		template<class F>
		explicit inline
		process(F f) {
			run(f);
		}

		process(const process&) = delete;

		process(pid_type rhs) noexcept:
		_pid(rhs)
		{}

		inline
		process(process&& rhs) noexcept:
		_pid(rhs._pid)
		{
			rhs._pid = 0;
		}

		~process() {
			if (_pid > 0) {
		   		do_kill(SIGTERM);
			}
		}

		inline
		process& operator=(process&& rhs) noexcept {
			_pid = rhs._pid;
			rhs._pid = 0;
			return *this;
		}


		template<class F>
		void
		run(F f) {
			_pid = ::fork();
			if (_pid == 0) {
				int ret = f();
				std::exit(ret);
			}
		}

		inline void stop() { signal(SIGSTOP); }
		inline void resume() { signal(SIGCONT); }
		inline void terminate() { signal(SIGTERM); }

		inline void
		signal(int sig) {
			if (_pid > 0) {
		    	bits::check(do_kill(sig),
					__FILE__, __LINE__, __func__);
			}
		}

		sysx::proc_status
		wait() {
			int stat = 0;
			if (_pid > 0) {
				bits::check_if_not<std::errc::interrupted>(
					::waitpid(_pid, &stat, 0),
					__FILE__, __LINE__, __func__);
				_pid = 0;
			}
			return sysx::proc_status(stat);
		}

		explicit inline
		operator bool() const noexcept {
			return _pid > 0 && do_kill(0) != -1;
		}

		inline bool
		operator !() const noexcept {
			return !operator bool();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const process& rhs) {
			return out << '{'
				<< "id=" << rhs.id()
				<< ",gid=" << rhs.group_id()
				<< '}';
		}

		pid_type
		id() const noexcept {
			return _pid;
		}

		pid_type
		group_id() const noexcept {
			return ::getpgid(_pid);
		}

		void
		set_group_id(pid_type rhs) const {
			bits::check(::setpgid(_pid, rhs),
				__FILE__, __LINE__, __func__);
		}

		/// std::thread interface
		bool joinable() { return true; }
		void join() { wait(); }

	private:

		inline int
		do_kill(int sig) const {
		   	return ::kill(_pid, sig);
		}

		pid_type _pid = 0;
	};

	struct process_group {

		typedef stdx::log<process_group> this_log;
		typedef std::vector<process>::iterator iterator;

		template<class F>
		const process&
		add(F child_main) {
			process p(child_main);
			if (_procs.empty()) {
				_gid = p.id();
			}
			p.set_group_id(_gid);
			_procs.push_back(std::move(p));
			return _procs.back();
		}

		template<class F>
		void
		emplace(F&& childmain) {
			_procs.emplace_back(std::forward<F>(childmain));
			process& proc = _procs.back();
			if (_procs.size() == 1) {
				_gid = proc.id();
			}
			proc.set_group_id(_gid);
		}

		int
		wait() {
			int ret = 0;
			for (process& p : _procs) {
				sysx::proc_status x = p.wait();
				this_log() << "process terminated:"
					<< stdx::make_fields("process", p, "exit_code", x.exit_code(), "term_signal", x.term_signal())
					<< std::endl;
				ret |= x.exit_code() | x.term_signal();
			}
			return ret;
		}

		template<class F>
		void
		wait(F callback, wait_flags flags=proc_exited) {
			while (!_procs.empty()) {
				sysx::proc_info status = do_wait(flags);
				auto result = std::find_if(
					_procs.begin(), _procs.end(),
					[&status] (const process& p) {
						return p.id() == status.pid();
					}
				);
				if (result != _procs.end()) {
					callback(*result, status);
					_procs.erase(result);
				}
			}
		}

		inline void
		stop() {
			if (_gid > 0) {
		    	bits::check(::kill(_gid, SIGTERM),
					__FILE__, __LINE__, __func__);
			}
		}

		inline pid_type
		id() const noexcept {
			return _gid;
		}

		const process&
		operator[](size_t i) const noexcept {
			return _procs[i];
		}

		process&
		operator[](size_t i) noexcept {
			return _procs[i];
		}

		size_t
		size() const noexcept {
			return _procs.size();
		}

		iterator
		begin() noexcept {
			return _procs.begin();
		}

		iterator
		end() noexcept {
			return _procs.end();
		}

		const process&
		front() const noexcept {
			return _procs.front();
		}

		process&
		front() noexcept {
			return _procs.front();
		}

		const process&
		back() const noexcept {
			return _procs.back();
		}

		process&
		back() noexcept {
			return _procs.back();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const process_group& rhs) {
			out << "{gid=" << rhs._gid << ",[";
			std::copy(rhs._procs.begin(), rhs._procs.end(),
				stdx::intersperse_iterator<process>(out, ","));
			out << "]}";
			return out;
		}

	private:

		sysx::proc_info
		do_wait(wait_flags flags) const {
			sysx::siginfo_type info;
			bits::check_if_not<std::errc::interrupted>(
				::waitid(P_PGID, _gid, &info, flags),
				__FILE__, __LINE__, __func__);
			return sysx::proc_info(info);
		}

		std::vector<process> _procs;
		pid_type _gid = 0;
	};

	namespace this_process {

		inline pid_type
		id() noexcept { return ::getpid(); }

		inline pid_type
		parent_id() noexcept { return ::getppid(); }

		inline pid_type
		group_id() noexcept { return ::getpgrp(); }

		inline void
		set_group_id(pid_type rhs) {
			bits::check(::setpgid(this_process::id(), rhs),
				__FILE__, __LINE__, __func__);
		}

		template<class T>
		T getenv(const char* key, T dflt) {
			const char* text = ::getenv(key);
			if (!text) { return dflt; }
			std::stringstream s;
			s << text;
			T val;
			return (s >> val) ? val : dflt;
		}

		// TODO: setenv() is known to cause memory leaks
		template<class T>
		void
		env(const char* key, T val) {
			std::string str = bits::to_string(val);
			bits::check(::setenv(key, str.c_str(), 1),
				__FILE__, __LINE__, __func__);
		}

		template<class ... Args>
		int
		execute(const Args& ... args) {
			bits::To_string tmp[] = { args... };
			const size_t argc = sizeof...(Args);
			char* argv[argc + 1];
			for (size_t i=0; i<argc; ++i) {
				argv[i] = const_cast<char*>(tmp[i].c_str());
			}
			argv[argc] = 0;
			return bits::check(::execv(argv[0], argv),
				__FILE__, __LINE__, __func__);
		}

		inline void
		bind_signal(int signum, const Action& action) {
			bits::check(::sigaction(signum, &action, 0),
				__FILE__, __LINE__, __func__);
		}

		const char*
		tempdir_path() {
			return "/tmp";
		}

	}

}

#endif // SYSX_PROCESS_HH
