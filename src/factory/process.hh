#ifndef FACTORY_PROCESS
#define FACTORY_PROCESS
namespace factory {

	namespace components {

		/// Fast mutex for scheduling strategies.
		struct Spin_mutex {
	
			void lock() noexcept { while (_flag.test_and_set(std::memory_order_acquire)); }
			void unlock() noexcept { _flag.clear(std::memory_order_release); }
			bool try_lock() noexcept { return !_flag.test_and_set(std::memory_order_acquire); }
	
			Spin_mutex() = default;
	
			// disallow copy & move operations
			Spin_mutex(const Spin_mutex&) = delete;
			Spin_mutex(Spin_mutex&&) = delete;
			Spin_mutex& operator=(const Spin_mutex&) = delete;
			Spin_mutex& operator=(Spin_mutex&&) = delete;
	
		private:
			std::atomic_flag _flag = ATOMIC_FLAG_INIT;
		};
	
	
	}
	typedef ::pid_t Process_id;
	namespace this_process {
	
		inline Process_id id() noexcept { return ::getpid(); }
		inline Process_id parent_id() noexcept { return ::getppid(); }
	
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

		typedef struct ::sigaction Basic_action;

		struct Action: public Basic_action {
			explicit Action(void (*func)(int)) noexcept {
				this->sa_handler = func;
			}
		};

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
			    	check(::kill(_child_pid, SIGHUP),
						__FILE__, __LINE__, __func__);
				}
			}

			int wait() {
				int ret = 0, sig = 0;
				if (_child_pid > 0) {
					int status = 0;
//					check("waitpid()", ::waitpid(_child_pid, &status, 0));
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

			static Process_id wait(int* status) {
				return check_if_not<std::errc::interrupted>(::wait(status),
					__FILE__, __LINE__, __func__);
			}

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

			template<class F>
			void wait(F callback) {
				int status = 0;
				int pid = check_if_not<std::errc::interrupted>(::wait(&status),
					__FILE__, __LINE__, __func__);
				auto result = std::find_if(_procs.begin(), _procs.end(),
					[pid] (Process p) {
						return p.id() == pid;
					}
				);
				if (result != _procs.end()) {
					callback(*result, status);
				}
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
				std::ostream_iterator<char*> it(cmdline, "\n");
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

		template<class T>
		struct shared_mem {
		
			typedef uint8_t proj_id_type;
			typedef ::key_t key_type;
			typedef size_t size_type;
			typedef unsigned short perms_type;
			typedef uint64_t id_type;
			typedef int shmid_type;
			typedef void* addr_type;
			typedef T value_type;
		
			shared_mem(id_type id, size_type sz, proj_id_type num, perms_type perms = DEFAULT_SHMEM_PERMS):
				_id(id), _size(sz), _key(this->genkey(num)),
				_shmid(this->createshmem(perms)),
				_addr(this->attach()), _owner(true)
			{
				this->fillshmem();
				Logger<Level::SHMEM>()
					<< "shared_mem server: "
					<< "shmem=" << *this
					<< std::endl;
			}
		
			shared_mem(id_type id, proj_id_type num):
				_id(id), _size(0), _key(this->genkey(num)),
				_shmid(this->getshmem()),
				_addr(this->attach())
			{
				this->_size = this->load_size();
				Logger<Level::SHMEM>()
					<< "shared_mem client: "
					<< "shmem=" << *this
					<< std::endl;
			}

			shared_mem(shared_mem&& rhs):
				_id(rhs._id),
				_size(rhs._size),
				_key(rhs._key),
				_shmid(rhs._shmid),
				_addr(rhs._addr),
				_owner(rhs._owner)
			{
				rhs._addr = nullptr;
				rhs._owner = false;
			}

			shared_mem() = default;
		
			~shared_mem() {
				this->detach();
				if (this->is_owner()) {
					this->rmshmem();
					this->rmfile();
				}
			}
		
			shared_mem(const shared_mem&) = delete;
			shared_mem& operator=(const shared_mem&) = delete;
		
			addr_type ptr() { return this->_addr; }
			const addr_type ptr() const { return this->_addr; }
			size_type size() const { return this->_size; }
			bool is_owner() const { return this->_owner; }

			value_type* begin() { return static_cast<value_type*>(this->_addr); }
			value_type* end() { return static_cast<value_type*>(this->_addr) + this->_size; }
		
			const value_type* begin() const { return static_cast<value_type*>(this->_addr); }
			const value_type* end() const { return static_cast<value_type*>(this->_addr) + this->_size; }

			void resize(size_type new_size) {
				if (new_size > this->_size) {
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_size = new_size;
					this->_shmid = this->createshmem(this->getperms());
					this->attach();
				}
			}

			void sync() {
				shmid_type newid = this->getshmem();
				if (newid != this->_shmid) {
					Logger<Level::SHMEM>()
						<< "detach/attach sync"
						<< std::endl;
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_shmid = newid;
					this->attach();
				}
				this->_size = this->load_size();
			}

			void create(id_type id, size_type sz, proj_id_type num, perms_type perms = DEFAULT_SHMEM_PERMS) {
				this->_id = id;
				this->_size = sz;
				this->_key = this->genkey(num);
				this->_shmid = this->createshmem(perms);
				this->_addr = this->attach();
				this->_owner = true;
				this->fillshmem();
				Logger<Level::SHMEM>()
					<< "shared_mem server: "
					<< "shmem=" << *this
					<< std::endl;
			}

			void attach(id_type id, proj_id_type num) {
				this->_id = id;
				this->_size = 0;
				this->_key = this->genkey(num);
				this->_shmid = this->getshmem();
				this->_addr = this->attach();
				this->_size = this->load_size();
				Logger<Level::SHMEM>()
					<< "shared_mem client: "
					<< "shmem=" << *this
					<< std::endl;
			}

		private:

			void fillshmem() {
				std::fill_n(this->begin(),
					this->size(), value_type());
			}

			key_type genkey(proj_id_type num) const {
				std::string path = filename(this->_id);
				{ std::filebuf out; out.open(path, std::ios_base::out); }
				return check(::ftok(path.c_str(), num),
					__FILE__, __LINE__, __func__);
			}

			shmid_type createshmem(perms_type perms) const {
				return check(::shmget(this->_key,
					this->size(), perms | IPC_CREAT),
					__FILE__, __LINE__, __func__);
			}

			shmid_type getshmem() const {
				return check(::shmget(this->_key, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			addr_type attach() const {
				return check(::shmat(this->_shmid, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			void detach() {
				if (this->_addr) {
					check(::shmdt(this->_addr),
						__FILE__, __LINE__, __func__);
				}
			}

			void rmshmem() {
				check(::shmctl(this->_shmid, IPC_RMID, 0),
					__FILE__, __LINE__, __func__);
			}

			void rmfile() {
				std::string path = this->filename(this->_id);
				Logger<Level::SHMEM>()
					<< "rmfile path=" << path
					<< ",shmem=" << *this
					<< std::endl;
				check(::remove(path.c_str()),
					__FILE__, __LINE__, __func__);
			}

			size_type load_size() {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_segsz;
			}

			perms_type getperms() {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_perm.mode;
			}
			
			static
			std::string filename(id_type id) {
				std::ostringstream path;
				path << "/var/tmp";
				path << "/factory.";
				path << std::hex << id;
				path << ".shmem";
				return path.str();
			}
		
			friend std::ostream& operator<<(std::ostream& out, const shared_mem& rhs) {
				return out << "{addr=" << rhs.ptr()
					<< ",size=" << rhs.size()
					<< ",owner=" << rhs.is_owner()
					<< ",key=" << rhs._key
					<< ",shmid=" << rhs._shmid
					<< '}';
			}
		
			id_type _id = 0;
			size_type _size = 0;
			key_type _key = 0;
			shmid_type _shmid = 0;
			addr_type _addr = nullptr;
			bool _owner = false;
		
			static const perms_type DEFAULT_SHMEM_PERMS = 0666;
		};

		struct Semaphore {

			typedef ::sem_t sem_type;
			typedef int flags_type;

			Semaphore() = default;

			explicit
			Semaphore(const std::string& name, bool owner=true):
				_name(name),
				_owner(owner),
				_sem(this->open_sem())
				{}

			Semaphore(Semaphore&& rhs):
				_name(std::move(rhs._name)),
				_owner(rhs._owner),
				_sem(rhs._sem)
			{
				rhs._sem = nullptr;
			}

			~Semaphore() {
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
		
			Semaphore& operator=(const Semaphore&) = delete;
			Semaphore(const Semaphore&) = delete;
		
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

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_shmembuf: public std::basic_streambuf<Ch,Tr> {

			using typename std::basic_streambuf<Ch,Tr>::int_type;
			using typename std::basic_streambuf<Ch,Tr>::traits_type;
			using typename std::basic_streambuf<Ch,Tr>::char_type;
			using typename std::basic_streambuf<Ch,Tr>::pos_type;
			using typename std::basic_streambuf<Ch,Tr>::off_type;
			typedef std::ios_base::openmode openmode;
			typedef std::ios_base::seekdir seekdir;
			typedef typename shared_mem<char_type>::size_type size_type;
			typedef typename shared_mem<char_type>::id_type id_type;
			typedef Spin_mutex mutex_type;
//			typedef std::lock_guard<mutex_type> lock_type;
			typedef typename shared_mem<Ch>::proj_id_type proj_id_type;

			explicit basic_shmembuf(id_type id):
				_sharedmem(id, 512, BUFFER_PROJID),
				_sharedpart(new (this->_sharedmem.ptr()) shmem_header)
			{
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->debug("basic_shmembuf()");
			}

			basic_shmembuf(id_type id, int):
				_sharedmem(id, BUFFER_PROJID),
				_sharedpart(reinterpret_cast<shmem_header*>(this->_sharedmem.ptr()))
			{
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->sync_sharedmem();
				this->debug("basic_shmembuf(int)");
			} 

			basic_shmembuf(basic_shmembuf&& rhs):
				_sharedmem(std::move(rhs._sharedmem)),
				_sharedpart(rhs._sharedpart)
				{}

			basic_shmembuf() = default;
			~basic_shmembuf() = default;

			void create(id_type id) {
				this->_sharedmem.create(id, 512, BUFFER_PROJID);
				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->writeoffs();
				this->debug("basic_shmembuf::create()");
			}
			
			void attach(id_type id) {
				this->_sharedmem.attach(id, BUFFER_PROJID),
//				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart = reinterpret_cast<shmem_header*>(this->_sharedmem.ptr());
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->sync_sharedmem();
				this->debug("basic_shmembuf::attach()");
			}

			int_type overflow(int_type c = traits_type::eof()) {
//				this->sync_sharedmem();
				int_type ret;
				if (c != traits_type::eof()) {
					this->grow_sharedmem();
					*this->pptr() = c;
					this->pbump(1);
					this->setg(this->eback(), this->gptr(), this->egptr()+1);
//					this->writeoffs();
					ret = traits_type::to_int_type(c);
				} else {
					ret = traits_type::eof();
				}
				return ret;
			}

			int_type underflow() {
				return this->gptr() == this->egptr()
					? traits_type::eof()
					: traits_type::to_int_type(*this->gptr());
			}

			int_type uflow() {
//				this->sync_sharedmem();
				if (this->underflow() == traits_type::eof()) return traits_type::eof();
				int_type c = traits_type::to_int_type(*this->gptr());
				this->gbump(1);
//				this->writeoffs();
				return c;
			}

			std::streamsize xsputn(const char_type* s, std::streamsize n) override {
				this->debug("xsputn");
				std::streamsize nwritten = 0;
				while (nwritten != n) {
					if (this->epptr() == this->pptr()) {
						this->overflow();
					}
					std::streamsize avail = static_cast<std::streamsize>(this->epptr() - this->pptr());
					std::streamsize m = std::min(n, avail);
					traits_type::copy(this->pptr(), s, m);
					nwritten += m;
					this->pbump(m);
				}
				return nwritten;
			}

			void lock() {
				this->mutex().lock();
				this->sync_sharedmem();
				this->debug("lock");
			}
			void unlock() {
				this->writeoffs();
				this->mutex().unlock();
				this->debug("unlock");
			}

		private:

			mutex_type& mutex() { return this->_sharedpart->mtx; }

			void debug(const char* msg) {
				Logger<Level::SHMEM>()
					<< msg << ": size1="
					<< this->_sharedpart->size
					<< ",goff=" << this->_sharedpart->goff
					<< ",poff=" << this->_sharedpart->poff
					<< ",pptr=" << static_cast<std::ptrdiff_t>(this->pptr() - this->pbase())
					<< ",shmem="
					<< this->_sharedmem
					<< std::endl;
			}

			void grow_sharedmem() {
				this->_sharedmem.resize(this->_sharedmem.size()
					* size_type(2));
				this->_sharedpart->size = this->_sharedmem.size();
				this->updatebufs();
			}
			
			void sync_sharedmem() {
				// update shared memory size
				if (this->bad_size()) {
					this->_sharedmem.sync();
//					this->_sharedpart->size = this->_sharedmem.size();
				}
				this->debug("sync_sharedmem");
				if (this->bad_size()) {
					throw Error("bad shared_mem size",
						__FILE__, __LINE__, __func__);
				}
				this->readoffs();
			}
			
			bool bad_size() const {
				return this->_sharedpart->size
					!= this->_sharedmem.size();
			}
			
			void updatebufs() {
				std::ptrdiff_t poff = this->pptr() - this->pbase();
				std::ptrdiff_t goff = this->gptr() - this->eback();
				char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
				char_type* end = this->_sharedmem.end();
				this->setp(base, end);
				this->pbump(poff);
				this->setg(base, base + goff, base + poff);
			}

			void writeoffs() {
				this->_sharedpart->goff = static_cast<pos_type>(this->gptr() - this->eback());
				this->_sharedpart->poff = static_cast<pos_type>(this->pptr() - this->pbase());
				this->debug("writeoffs");
			}

			void readoffs() {
				pos_type goff = this->_sharedpart->goff;
				pos_type poff = this->_sharedpart->poff;
				char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
				char_type* end = this->_sharedmem.end();
				this->setp(base, end);
				this->pbump(poff);
				this->setg(base, base + goff, base + poff);
				this->debug("readoffs");
			}

			shared_mem<char_type> _sharedmem;
			struct shmem_header {
				size_type size = 0;
				mutex_type mtx;
				pos_type goff = 0;
				pos_type poff = 0;
			} *_sharedpart = nullptr;

			static const proj_id_type HEADER_PROJID = 'h';
			static const proj_id_type BUFFER_PROJID = 'b';
		};

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_ikernelshmembuf: public basic_ikernelbuf<basic_shmembuf<Ch,Tr>> {
			typedef basic_ikernelbuf<basic_shmembuf<Ch,Tr>> base_type;
			typedef typename base_type::id_type id_type;
			basic_ikernelshmembuf() = default;
			explicit basic_ikernelshmembuf(id_type id) {
				this->base_type::create(id);
			}
			basic_ikernelshmembuf(id_type id, int unused) {
				this->base_type::attach(id);
			}
		};

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_okernelshmembuf: public basic_okernelbuf<basic_shmembuf<Ch,Tr>> {
			typedef basic_okernelbuf<basic_shmembuf<Ch,Tr>> base_type;
			typedef typename base_type::id_type id_type;
			basic_okernelshmembuf() = default;
			explicit basic_okernelshmembuf(id_type id) {
				this->base_type::create(id);
			}
			basic_okernelshmembuf(id_type id, int unused) {
				this->base_type::attach(id);
			}
		};

	}

	namespace this_process {

		// TODO: setenv() is known to cause memory leaks
		template<class T>
		void env(const char* key, T val) {
			components::check(::setenv(key, components::To_string(val).c_str(), 1),
				__FILE__, __LINE__, __func__);
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
			return components::check(::execv(argv[0], argv), 
				__FILE__, __LINE__, __func__);
		}

		inline
		void bind_signal(int signum, const components::Action& action) {
			components::check(::sigaction(signum, &action, 0),
				__FILE__, __LINE__, __func__);
		}

	}

}
#endif
