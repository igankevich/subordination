#include "bits/unistdx.hh"
#include "bits/ifaddrs.hh"
#include "bits/endpoint.hh"

#include "unistdx/types.hh"
#include "unistdx/endpoint.hh"
#include "unistdx/fildes.hh"
#include "unistdx/event.hh"
#include "unistdx/process.hh"

namespace factory {

	namespace unix {

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

			typedef ::sem_t* sem_type;
			typedef int flags_type;
			typedef std::string path_type;

			semaphore() = default;

			explicit
			semaphore(path_type&& name, mode_type mode):
				_path(std::forward<path_type>(name)),
				_owner(true),
				_sem(this->open_sem(mode))
				{}

			explicit
			semaphore(path_type&& name):
				_path(std::forward<path_type>(name)),
				_owner(false),
				_sem(this->open_sem())
				{}

			semaphore(semaphore&& rhs) noexcept:
				_path(std::move(rhs._path)),
				_owner(rhs._owner),
				_sem(rhs._sem)
			{
				rhs._sem = nullptr;
			}

			~semaphore() {
				this->close();
			}
		
			semaphore(const semaphore&) = delete;
			semaphore& operator=(const semaphore&) = delete;

			void open(path_type&& name, mode_type mode) {
				this->close();
				this->_path = std::forward<path_type>(name),
				this->_owner = true;
				this->_sem = this->open_sem(mode);
			}

			void open(path_type&& name) {
				this->close();
				this->_path = std::forward<path_type>(name),
				this->_owner = false;
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

			void notify_all() {
				for (int i=0; i<1000; ++i) {
					notify_one();
				}
			}

			void close() {
				if (this->_sem != SEM_FAILED) {
					check(::sem_close(this->_sem),
						__FILE__, __LINE__, __func__);
					if (this->_owner) {
						check(::sem_unlink(this->_path.c_str()),
							__FILE__, __LINE__, __func__);
					}
				}
			}
		
		private:

			sem_type
			open_sem(mode_type mode) const {
				return check(::sem_open(this->_path.c_str(),
					O_CREAT | O_EXCL, mode, 0), SEM_FAILED,
					__FILE__, __LINE__, __func__);
			}

			sem_type
			open_sem() const {
				return check(::sem_open(this->_path.c_str(), 0),
					SEM_FAILED, __FILE__, __LINE__, __func__);
			}

			path_type _path;
			bool _owner = false;
			sem_type _sem = SEM_FAILED;
		};

		struct thread_semaphore {

			typedef ::sem_t sem_type;

			thread_semaphore():
			_sem()
			{
				init_sem();
			}

			~thread_semaphore() {
				destroy();
			}

			void wait() {
				check(::sem_wait(&_sem),
					__FILE__, __LINE__, __func__);
			}

			template<class Mutex>
			void wait(Mutex& mtx) {
				mtx.unlock();
				wait();
				mtx.lock();
			}

			template<class Mutex, class Pred>
			void wait(Mutex& mtx, Pred pred) {
				while (!pred()) {
					wait(mtx);
				}
			}

			void notify_one() {
				check(::sem_post(&_sem),
					__FILE__, __LINE__, __func__);
			}

			void notify_all() {
				for (int i=0; i<1000; ++i) {
					notify_one();
				}
			}

		private:

			inline void
			destroy() {
				check(::sem_destroy(&_sem),
					__FILE__, __LINE__, __func__);
			}

			inline void
			init_sem() {
				check(::sem_init(&_sem, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			sem_type _sem;
		};

		template<class T>
		struct shared_mem {
		
			typedef uint8_t proj_id_type;
			typedef ::key_t key_type;
			typedef size_t size_type;
			typedef int shmid_type;
			typedef void* addr_type;
			typedef T value_type;
			typedef T* iterator;
			typedef const T* const_iterator;
			typedef std::string path_type;

			shared_mem(path_type&& name, size_type sz, mode_type mode, proj_id_type num = DEFAULT_PROJ_ID):
				_path(this->make_path(std::forward<path_type>(name))),
				_size(sz), _key(this->genkey(num)),
				_shmid(this->open_shmem(mode)),
				_addr(this->attach()), _owner(true)
			{
				this->fillshmem();
//				Logger<Level::SHMEM>()
//					<< "shared_mem server: "
//					<< "shmem=" << *this
//					<< std::endl;
			}

			explicit
			shared_mem(path_type&& name, proj_id_type num = DEFAULT_PROJ_ID):
				_path(this->make_path(std::forward<path_type>(name))),
				_size(0), _key(this->genkey(num)),
				_shmid(this->getshmem()),
				_addr(this->attach()),
				_owner(false)
			{
				this->_size = this->load_size();
//				Logger<Level::SHMEM>()
//					<< "shared_mem client: "
//					<< "shmem=" << *this
//					<< std::endl;
			}
		
			shared_mem(shared_mem&& rhs):
				_path(std::move(rhs._path)),
				_size(rhs._size),
				_key(rhs._key),
				_shmid(rhs._shmid),
				_addr(rhs._addr),
				_owner(rhs._owner)
			{
				rhs._addr = nullptr;
				rhs._owner = false;
			}

			shared_mem& operator=(shared_mem&& rhs) {
				this->close();
				this->_path = std::move(rhs._path);
				this->_size = rhs._size;
				this->_key = rhs._key;
				this->_shmid = rhs._shmid;
				this->_addr = rhs._addr;
				this->_owner = rhs._owner;
				rhs._addr = nullptr;
				rhs._owner = false;
				return *this;
			}

			shared_mem() = default;
		
			~shared_mem() {
				this->close();
			}
		
			shared_mem(const shared_mem&) = delete;
			shared_mem& operator=(const shared_mem&) = delete;
		
			inline addr_type
			ptr() noexcept {
				return this->_addr;
			}

			inline const addr_type
			ptr() const noexcept {
				return this->_addr;
			}

			inline size_type
			size() const noexcept {
				return this->_size;
			}

			inline bool
			is_owner() const noexcept {
				return this->_owner;
			}

			inline iterator
			begin() noexcept {
				return static_cast<iterator>(this->_addr);
			}

			inline iterator
			end() noexcept {
				return static_cast<iterator>(this->_addr) + this->_size;
			}
		
			inline const_iterator
			begin() const noexcept {
				return static_cast<iterator>(this->_addr);
			}

			inline const_iterator
			end() const noexcept {
				return static_cast<iterator>(this->_addr) + this->_size;
			}

			void resize(size_type new_size) {
				if (new_size > this->_size) {
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_size = new_size;
					this->_shmid = this->open_shmem(this->getmode());
					this->attach();
				}
			}

			void sync() {
				shmid_type newid = this->getshmem();
				if (newid != this->_shmid) {
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_shmid = newid;
					this->attach();
				}
				this->_size = this->load_size();
			}

			void open(path_type&& name, size_type sz, mode_type mode, proj_id_type num) {
				this->close();
				this->_path = this->make_path(std::forward<path_type>(name));
				this->_size = sz;
				this->_key = this->genkey(num);
				this->_shmid = this->open_shmem(mode);
				this->_addr = this->attach();
				this->_owner = true;
				this->fillshmem();
//				Logger<Level::SHMEM>()
//					<< "shared_mem server: "
//					<< "shmem=" << *this
//					<< std::endl;
			}

			void attach(path_type&& name, proj_id_type num) {
				this->close();
				this->_path = this->make_path(std::forward<path_type>(name));
				this->_size = 0;
				this->_key = this->genkey(num);
				this->_shmid = this->getshmem();
				this->_addr = this->attach();
				this->_size = this->load_size();
//				Logger<Level::SHMEM>()
//					<< "shared_mem client: "
//					<< "shmem=" << *this
//					<< std::endl;
			}

			void close() {
				this->detach();
				if (this->is_owner()) {
					this->rmshmem();
					this->rmfile();
				}
			}

		private:

			void fillshmem() noexcept {
				std::fill_n(this->begin(),
					this->size(), value_type());
			}

			key_type genkey(proj_id_type num) const {
				{ std::filebuf out; out.open(this->_path, std::ios_base::out); }
				return check(::ftok(this->_path.c_str(), num),
					__FILE__, __LINE__, __func__);
			}

			shmid_type open_shmem(mode_type mode) const {
				return check(::shmget(this->_key,
					this->size(), mode | IPC_CREAT),
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
					this->_addr = nullptr;
				}
			}

			void rmshmem() {
				check(::shmctl(this->_shmid, IPC_RMID, 0),
					__FILE__, __LINE__, __func__);
			}

			void rmfile() {
				check(::remove(this->_path.c_str()),
					__FILE__, __LINE__, __func__);
			}

			size_type
			load_size() const {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_segsz;
			}

			mode_type
			getmode() const {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_perm.mode;
			}

			static path_type
			make_path(path_type&& rhs) {
				return path_type(this_process::tempdir_path()
					+ std::forward<path_type>(rhs));
			}
			
			friend std::ostream&
			operator<<(std::ostream& out, const shared_mem& rhs) {
				return out << "{addr=" << rhs.ptr()
					<< ",path=" << rhs._path
					<< ",size=" << rhs.size()
					<< ",owner=" << rhs.is_owner()
					<< ",key=" << rhs._key
					<< ",shmid=" << rhs._shmid
					<< '}';
			}
		
			std::string _path;
			size_type _size = 0;
			key_type _key = 0;
			shmid_type _shmid = 0;
			addr_type _addr = nullptr;
			bool _owner = false;
		
			constexpr static const proj_id_type DEFAULT_PROJ_ID = 'a';
		};

	}

}
