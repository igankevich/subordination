#ifndef FACTORY_PROCESS
#define FACTORY_PROCESS
namespace factory {

	/// Fast mutex for scheduling strategies.
	struct Spin_mutex {

		void lock() noexcept { while (_flag.test_and_set(std::memory_order_acquire)); }
		void unlock() noexcept { _flag.clear(std::memory_order_release); }
		bool try_lock() noexcept { return !_flag.test_and_set(std::memory_order_acquire); }

		constexpr Spin_mutex() {}
		Spin_mutex(const Spin_mutex&) = delete;
		Spin_mutex& operator=(const Spin_mutex&) = delete;

	private:
		std::atomic_flag _flag = ATOMIC_FLAG_INIT;
	};

	typedef ::pid_t Process_id;

	namespace this_process {

		Process_id id() { return ::getpid(); }
		Process_id parent_id() { return ::getppid(); }

		template<class T>
		T getenv(const char* key, T dflt) {
			const char* text = ::getenv(key);
			if (!text) { return dflt; }
			std::stringstream s;
			s << text;
			T val;
			return (s >> val) ? val : dflt;
		}

	}

}
#else
namespace factory {

	namespace components {
		struct To_string {

			template<class T>
			To_string(T rhs): _s(to_string(rhs)) {}

			const char* c_str() const { return _s.c_str(); }

		private:

			template<class T>
			static std::string to_string(T rhs) {
				std::stringstream s;
				s << rhs;
				return s.str();
			}

			std::string _s;
		};
	}

	namespace this_process {

		// TODO: setenv() is known to cause memory leaks
		template<class T>
		void env(const char* key, T val) {
			check("setenv()", ::setenv(key, components::To_string(val).c_str(), 1));
		}

		template<class ... Args>
		int execute(const Args& ... args) {
			components::To_string tmp[] = { args... };
			const size_t argc = sizeof...(Args);
			char* argv[argc + 1];
			for (size_t i=0; i<argc; ++i) {
				argv[i] = const_cast<char*>(tmp[i].c_str());
			}
			argv[argc] = 0;
			Logger<Level::COMPONENT> log;
			log << "Executing ";
			std::ostream_iterator<char*> it(log.ostream(), " ");
			std::copy(argv, argv + argc, it);
			return check("execve()", ::execve(argv[0], argv, ::environ));
		}

	}

	struct Process {

		constexpr Process() {}

		template<class F>
		explicit Process(F f) { run(f); }

		template<class F>
		void run(F f) {
			_child_pid = ::fork();
			if (_child_pid == 0) {
				int ret = f();
				Logger<Level::COMPONENT>() << ": exit(" << ret << ')' << std::endl;
				std::exit(ret);
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
//				check("waitpid()", ::waitpid(_child_pid, &status, 0));
				::waitpid(_child_pid, &status, 0);
				ret = WEXITSTATUS(status);
				if (WIFSIGNALED(status)) {
					sig = WTERMSIG(status);
				}
				Logger<Level::COMPONENT>()
					<< _child_pid << ": waitpid(), ret=" << ret
					<< ", sig=" << sig << std::endl;
			}
			return ret | sig;
		}

		friend std::ostream& operator<<(std::ostream& out, const Process& rhs) {
			return out << rhs.id();
		}

		constexpr Process_id id() const { return _child_pid; }

	private:

		Process_id _child_pid = 0;
	};

	struct Process_group {

		template<class F>
		void add(F f) {
			_procs.push_back(Process(f));
		}

		int wait() {
			int ret = 0;
			for (Process& p : _procs) ret += std::abs(p.wait());
			return ret;
		}

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

	struct Command_line {

		Command_line(int argc, char* argv[]): cmdline() {
			std::ostream_iterator<char*> it(cmdline, " ");
			std::copy(argv + 1, argv + argc, it);
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

	struct Shared_memory {
	
		typedef unsigned char Byte;
		typedef uint8_t Sequence_num;
	
		Shared_memory(int id, size_t sz, Sequence_num num = PROJ_ID, int perms = DEFAULT_SHMEM_PERMS):
			_addr(nullptr), _id(id), _size(sz), _shmid(0), _owner(true)
		{
			_path = filename(id);
			{ std::ofstream out(_path); }
			::key_t key = check("ftok()", ::ftok(_path.c_str(), num));
			_shmid = check("shmget()", ::shmget(key, size(), perms | IPC_CREAT));
			_addr = static_cast<Byte*>(check("shmat()", ::shmat(_shmid, 0, 0)));
		}
	
		explicit Shared_memory(int id, Sequence_num num = PROJ_ID):
			_id(id), _size(0), _shmid(0), _path()
		{
			_path = filename(id);
			{ std::filebuf out; out.open(_path, std::ios_base::out); }
			::key_t key = check("ftok()", ::ftok(_path.c_str(), num));
			_shmid = check("shmget()", ::shmget(key, 0, 0));
			_addr = static_cast<Byte*>(check("shmat()", ::shmat(_shmid, 0, 0)));
			load_size();
		}
	
		~Shared_memory() {
			check("shmdt()", ::shmdt(_addr));
			if (is_owner()) {
				check("shmctl()", ::shmctl(_shmid, IPC_RMID, 0));
				check("remove()", ::remove(_path.c_str()));
			}
		}
	
		Shared_memory(const Shared_memory&) = delete;
		Shared_memory& operator=(const Shared_memory&) = delete;
	
		unsigned char* ptr() { return _addr; }
		const unsigned char* ptr() const { return _addr; }
		size_t size() const { return _size; }
		bool is_owner() const { return _owner; }
	
	private:
	
		void load_size() {
			::shmid_ds stat;
			check("shmctl()", ::shmctl(_shmid, IPC_STAT, &stat));
			_size = stat.shm_segsz;
		}
		
		std::string filename(int id) const {
			std::stringstream path;
			path << "/var/tmp";
			path << "/factory.";
			path << id;
			path << ".shmem";
			return path.str();
		}
	
		friend std::ostream& operator<<(std::ostream& out, const Shared_memory& rhs) {
			return out << "addr = " << reinterpret_cast<const void*>(rhs.ptr())
				<< ", size = " << rhs.size()
				<< ", owner = " << rhs.is_owner()
				<< ", path = " << rhs._path;
		}
	
		Byte* _addr;
		int _id = 0;
		size_t _size = 0;
		int _shmid = 0;
		std::string _path;
		bool _owner = false;
	
		static const int DEFAULT_SHMEM_PERMS = 0666;
		static const uint8_t PROJ_ID = 0;
	};

	struct Semaphore {

		explicit Semaphore(const std::string& name): Semaphore(name.c_str()) {}
		explicit Semaphore(const char* name) {
			_sem = check("sem_open()", ::sem_open(name, O_CREAT, 0666, 0), SEM_FAILED);
		}
		~Semaphore() { check("sem_close()", ::sem_close(_sem)); }

		void wait() {
			std::cout << "sem = " << _sem << std::endl;
			check("sem_wait()", ::sem_wait(_sem));
		}
		void notify_one() { check("sem_post()", ::sem_post(_sem)); }
		void lock() { this->wait(); }
		void unlock() { this->notify_one(); }
	
		Semaphore& operator=(const Semaphore&) = delete;
		Semaphore(const Semaphore&) = delete;
	
	private:
		::sem_t* _sem;
	};

}
#endif
