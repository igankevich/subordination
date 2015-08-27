#ifndef FACTORY_UNISTDX_SHAREDMEM_HH
#define FACTORY_UNISTDX_SHAREDMEM_HH

#include <unistd.h>
#include <sys/shm.h>
#include <cstdio>
// TODO: replace remove() with unlink() and remove stdio.h dependency

#include <stdx/log.hh>

#include <sysx/fildes.hh>
#include <sysx/process.hh>

namespace sysx {

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
		typedef stdx::log<shared_mem> this_log;

		shared_mem(path_type&& name, size_type sz, mode_type mode, proj_id_type num = DEFAULT_PROJ_ID):
			_path(this->make_path(std::forward<path_type>(name))),
			_size(sz), _key(this->genkey(num)),
			_shmid(this->open_shmem(mode)),
			_addr(this->attach()), _owner(true)
		{
			this->fillshmem();
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
		}

		void attach(path_type&& name, proj_id_type num) {
			this->close();
			this->_path = this->make_path(std::forward<path_type>(name));
			this->_size = 0;
			this->_key = this->genkey(num);
			this->_shmid = this->getshmem();
			this->_addr = this->attach();
			this->_size = this->load_size();
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
			{ file f(_path, file::read_only, file::create); }
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
			check(std::remove(this->_path.c_str()),
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

#endif // FACTORY_UNISTDX_SHAREDMEM_HH
