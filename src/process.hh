#include <sys/wait.h>

namespace factory {

	template<class In>
	void intersperse(In first, In last, std::ostream& result, const char* delim) {
		if (first != last) {
			result << *first;
			++first;
		}
		while (first != last) {
			result << delim << *first;
			++first;
		}
	}

	typedef ::pid_t Process_id;

	struct Process {

		Process() {}

		template<class F>
		explicit Process(F f) {
			run(f);
		}

		template<class F>
		void run(F f) {
			_child_pid = ::fork();
			if (_child_pid == 0) {
				int ret = f();
				Logger(Level::COMPONENT)
					<< ::getpid() << ": exit(" << ret << ')' << std::endl;
				::exit(ret);
			} else {
//				std::clog << "fork " << id() << std::endl;
			}
		}

		void stop() {
			if (_child_pid > 0) {
		    	check("kill()", ::kill(_child_pid, SIGHUP));
			}
		}

		int wait() {
			int ret = 0, sig = 0;
			if (_child_pid > 0) {
				int status = 0;
				check("waitpid()", ::waitpid(_child_pid, &status, 0));
				ret = WEXITSTATUS(status);
				if (WIFSIGNALED(status)) {
					sig = WTERMSIG(status);
				}
				Logger(Level::COMPONENT)
					<< _child_pid << ": waitpid(), ret=" << ret
					<< ", sig=" << sig << std::endl;
			}
			return ret | sig;
		}

		friend std::ostream& operator<<(std::ostream& out, const Process& rhs) {
			return out << rhs.id();
		}

		Process_id id() const { return _child_pid; }

		template<class ... Args>
		static int execute(const Args& ... args) {
			To_string tmp[] = { args... };
			const int argc = sizeof...(Args);
			char* argv[argc + 1];
			for (int i=0; i<argc; ++i) {
				argv[i] = tmp[i].c_str();
			}
			argv[argc] = 0;
			Logger log(Level::COMPONENT);
			log << "Executing ";
			std::ostream_iterator<char*> it(log.ostream(), " ");
			std::copy(argv, argv + argc, it);
			return check("execve()", ::execve(argv[0], argv, ::environ));
		}

		template<class T>
		static void env(const char* key, T val) {
			check("setenv()", ::setenv(key, To_string(val).c_str(), 1));
		}

		template<class T>
		static T getenv(const char* key, T def) {
			const char* text = ::getenv(key);
			if (!text) { return def; }
			std::stringstream s;
			s << text;
			T val;
			return (s >> val) ? val : def;
		}

	private:
		struct To_string {

			template<class T>
			To_string(T rhs): _s(to_string(rhs)) {}

			char* c_str() { return (char*)_s.c_str(); }

		private:

			template<class T>
			std::string to_string(T rhs) {
				std::stringstream s;
				s << rhs;
				return s.str();
			}
			std::string _s;
		};

		Process_id _child_pid = 0;
	};

	struct Process_group {

		template<class F>
		void add(F f) {
			_processes.push_back(Process(f));
		}

		int wait(bool use_threads=false) {
			int ret = 0;
			if (use_threads) {
				std::atomic<int> rett(0);
				std::vector<std::thread> threads(_processes.size());
				for (size_t i=0; i<threads.size(); ++i) {
					threads[i] = std::thread([this,i,&rett]() {
						rett |= _processes[i].wait();
					});
				}
				for (size_t i=0; i<threads.size(); ++i) {
					if (threads[i].joinable()) {
						threads[i].join();
					}
				}
				ret = rett;
			} else {
				for (Process& p : _processes) ret += std::abs(p.wait());
			}
			return ret;
		}

		void stop() {
			for (Process& p : _processes)
				p.stop();
		}

		Process operator[](size_t i) { return _processes[i]; }

		friend std::ostream& operator<<(std::ostream& out, const Process_group& rhs) {
			for (const Process& p : rhs._processes) {
				out << p << ' ';
			}
			return out;
		}

	private:
		std::vector<Process> _processes;
	};



}
