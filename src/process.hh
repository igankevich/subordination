#include <sys/wait.h>

namespace factory {

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
//				std::clog << "exit " << ::getpid() << std::endl;
				std::clog << ::getpid() << ": exit(" << ret << ')' << std::endl;
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
			int ret = 0;
			if (_child_pid > 0) {
				check("waitpid()", ::waitpid(_child_pid, &ret, 0));
				ret = WEXITSTATUS(ret);
				std::clog << _child_pid << ": waitpid() = " << ret << std::endl;
			}
			return ret;
		}

		Process_id id() const { return _child_pid; }

	private:
		Process_id _child_pid = 0;
	};

	struct Process_group {

		template<class F>
		void add(F f) {
			_processes.push_back(Process(f));
		}

		int wait() {
			int ret = 0;
			std::for_each(_processes.begin(), _processes.end(), [&ret] (Process& p) {
				ret |= p.wait();
			});
			return ret;
		}

		void stop() {
			std::for_each(_processes.begin(), _processes.end(), [] (Process& p) {
				p.stop();
			});
		}

		Process operator[](size_t i) { return _processes[i]; }

	private:
		std::vector<Process> _processes;
	};

}
