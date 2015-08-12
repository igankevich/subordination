#include "bits/unistdext.hh"
namespace factory {

	typedef ::pid_t pid_type;
	typedef struct ::sigaction sigaction_type;

	namespace components {

		struct Process {

			constexpr Process() = default;

			template<class F>
			explicit
			Process(F f) { run(f); }

			template<class F>
			void run(F f) {
				_child_pid = ::fork();
				if (_child_pid == 0) {
					int ret = f();
//					Logger<Level::COMPONENT>() << ": exit(" << ret << ')' << std::endl;
					std::exit(ret);
				}
			}

			void stop() {
				if (_child_pid > 0) {
			    	check(::kill(_child_pid, SIGHUP),
						__FILE__, __LINE__, __func__);
				}
			}

			std::pair<int,int>
			wait() {
				int ret = 0, sig = 0;
				if (_child_pid > 0) {
					int status = 0;
					check_if_not<std::errc::interrupted>(
						::waitpid(_child_pid, &status, 0),
						__FILE__, __LINE__, __func__);
					ret = WEXITSTATUS(status);
					if (WIFSIGNALED(status)) {
						sig = WTERMSIG(status);
					}
//					Logger<Level::COMPONENT>()
//						<< _child_pid << ": waitpid(), ret=" << ret
//						<< ", sig=" << sig << std::endl;
				}
				return std::make_pair(ret, sig);
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Process& rhs) {
				return out << rhs.id();
			}

			constexpr pid_type
			id() const noexcept {
				return _child_pid;
			}

		private:
			pid_type _child_pid = 0;
		};

		struct Process_group {

			template<class F>
			void add(F f) {
				_procs.push_back(Process(f));
			}

			int wait() {
				int ret = 0;
				for (Process& p : _procs) {
					std::pair<int,int> x = p.wait();
					ret += std::abs(x.first | x.second);
				}
				return ret;
			}

/*
			template<class F>
			void wait(F callback) {
				int status = 0;
				int pid = check_if_not<std::errc::interrupted>(
					::wait(&status),
					__FILE__, __LINE__, __func__);
				auto result = std::find_if(_procs.begin(), _procs.end(),
					[pid] (Process p) {
						return p.id() == pid;
					}
				);
				if (result != _procs.end()) {
					int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 0,
						sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
					callback(*result, ret, sig);
				}
			}
*/

			void stop() {
				for (Process& p : _procs)
					p.stop();
			}

			Process& operator[](size_t i) { return _procs[i]; }
			const Process& operator[](size_t i) const { return _procs[i]; }

			friend std::ostream& operator<<(std::ostream& out, const Process_group& rhs) {
				for (const Process& p : rhs._procs) {
					out << p << ' ';
				}
				return out;
			}

		private:
			std::vector<Process> _procs;
		};

		struct Action: public sigaction_type {
			inline explicit
			Action(void (*func)(int)) noexcept {
				this->sa_handler = func;
			}
		};

		struct Command_line {

			Command_line(int argc, char* argv[]) noexcept:
				cmdline()
			{
				std::copy_n(argv, argc,
					std::ostream_iterator<char*>(cmdline, "\n"));
			}

			template<class F>
			void parse(F handle) {
				std::string arg;
				while (cmdline >> std::ws >> arg) {
					handle(arg, cmdline);
				}
			}

		private:
			std::stringstream cmdline;
		};

		struct semaphore {

			typedef ::sem_t sem_type;
			typedef int flags_type;

			semaphore() = default;

			explicit
			semaphore(const std::string& name, bool owner=true):
				_name(name),
				_owner(owner),
				_sem(this->open_sem())
				{}

			semaphore(semaphore&& rhs):
				_name(std::move(rhs._name)),
				_owner(rhs._owner),
				_sem(rhs._sem)
			{
				rhs._sem = nullptr;
			}

			~semaphore() {
				if (this->_sem) {
					check(::sem_close(this->_sem),
						__FILE__, __LINE__, __func__);
					if (this->_owner) {
						check(::sem_unlink(this->_name.c_str()),
							__FILE__, __LINE__, __func__);
					}
				}
			}

			void open(const std::string& name, bool owner=true) {
				this->_name = name;
				this->_owner = owner;
				this->_sem = this->open_sem();
			}

			void wait() {
				check(::sem_wait(this->_sem),
					__FILE__, __LINE__, __func__);
			}

			void notify_one() {
				check(::sem_post(this->_sem),
					__FILE__, __LINE__, __func__);
			}
		
			semaphore& operator=(const semaphore&) = delete;
			semaphore(const semaphore&) = delete;
		
		private:

			sem_type* open_sem() {
				return check(::sem_open(this->_name.c_str(),
					this->determine_flags(), 0666, 0), SEM_FAILED,
					__FILE__, __LINE__, __func__);
			}

			flags_type determine_flags() const {
				return this->_owner ? (O_CREAT | O_EXCL) : 0;
			}

			std::string _name;
			bool _owner = false;
			sem_type* _sem = nullptr;
		};
	
	}

	namespace this_process {
	
		inline pid_type
		id() noexcept { return ::getpid(); }

		inline pid_type
		parent_id() noexcept { return ::getppid(); }
	
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
		void env(const char* key, T val) {
			std::string str = bits::to_string(val);
			components::check(::setenv(key, str.c_str(), 1),
				__FILE__, __LINE__, __func__);
		}

		template<class ... Args>
		int execute(const Args& ... args) {
			bits::To_string tmp[] = { args... };
			const size_t argc = sizeof...(Args);
			char* argv[argc + 1];
			for (size_t i=0; i<argc; ++i) {
				argv[i] = const_cast<char*>(tmp[i].c_str());
			}
			argv[argc] = 0;
			return components::check(::execv(argv[0], argv), 
				__FILE__, __LINE__, __func__);
		}

		inline void
		bind_signal(int signum, const components::Action& action) {
			components::check(::sigaction(signum, &action, 0),
				__FILE__, __LINE__, __func__);
		}

		pid_type
		wait(int* status) {
			using components::check_if_not;
			return check_if_not<std::errc::interrupted>(
				::wait(status),
				__FILE__, __LINE__, __func__);
		}

	}

}
