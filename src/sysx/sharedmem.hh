#ifndef SYSX_SHAREDMEM_HH
#define SYSX_SHAREDMEM_HH

#include <memory>
#include <vector>
#include <fstream>

#include <unistd.h>
#include <sys/shm.h>

#include <stdx/log.hh>

#include <sysx/fildes.hh>
#include <sysx/process.hh>

namespace sysx {

	template<class T>
	struct shared_mem {

		typedef ::key_t key_type;
		typedef size_t size_type;
		typedef int shm_type;
		typedef void* addr_type;
		typedef T value_type;
		typedef T* iterator;
		typedef const T* const_iterator;
		typedef std::string path_type;

		shared_mem(path_type&& name, size_type min_sz, mode_type mode, char num = NO_PROJ_ID):
		_path(), _size(min_sz), _owner(true)
		{
			do_open(std::move(name), mode|IPC_CREAT, num);
		}

		explicit
		shared_mem(path_type&& name, char num = NO_PROJ_ID):
		_path(), _size(0), _owner(false)
		{
			do_open(std::move(name), 0, num);
		}

		shared_mem(size_type min_sz, mode_type mode):
		_path(), _size(min_sz), _owner(true)
		{
			do_open_private(IPC_PRIVATE, mode|IPC_CREAT);
		}

		explicit
		shared_mem(shm_type shm):
		_path(), _size(0), _shm(shm), _owner(false)
		{
			do_open_existing();
		}

		shared_mem(shared_mem&& rhs):
		_path(std::move(rhs._path)),
		_size(rhs._size),
		_key(rhs._key),
		_shm(rhs._shm),
		_addr(rhs._addr),
		_owner(rhs._owner)
		{
			rhs._addr = nullptr;
			rhs._owner = false;
		}

		shared_mem&
		operator=(shared_mem&& rhs) {
			this->close();
			_path = std::move(rhs._path);
			_size = rhs._size;
			_key = rhs._key;
			_shm = rhs._shm;
			_addr = rhs._addr;
			_owner = rhs._owner;
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

		addr_type
		ptr() noexcept {
			return _addr;
		}

		const addr_type
		ptr() const noexcept {
			return _addr;
		}

		size_type
		size() const noexcept {
			return _size;
		}

		bool
		is_owner() const noexcept {
			return _owner;
		}

		iterator
		begin() noexcept {
			return static_cast<iterator>(_addr);
		}

		iterator
		end() noexcept {
			return static_cast<iterator>(_addr) + _size;
		}

		const_iterator
		begin() const noexcept {
			return static_cast<iterator>(_addr);
		}

		const_iterator
		end() const noexcept {
			return static_cast<iterator>(_addr) + _size;
		}

		// TODO: resize is slow
		void
		resize(size_type new_size) {
			// copy old data and openmode
			const size_type n = std::min(new_size, _size);
			const std::vector<value_type> olddata(begin(), begin() + n);
			const mode_type mod = xmode();
			// detach and close old segment
			xdetach();
			xclose();
			_size = new_size;
			_shm = xopen(mod|IPC_CREAT);
			_addr = xattach();
			std::uninitialized_copy(olddata.begin(),
				olddata.end(), begin());
			_size = xsize();
		}

		void
		sync() {
			shm_type newid = xopen();
			if (newid != _shm) {
				xdetach();
				xclose();
				_shm = newid;
				_addr = xattach();
			}
			_size = xsize();
		}

		shm_type id() const { return _shm; }

	private:

		void
		do_open_existing() {
			_path.clear();
			_key = getkey();
			_addr = xattach();
			if (_owner) {
				xfill();
			}
			_size = xsize();
		}

		void
		do_open_private(key_type key, mode_type mode)
		{
			_path.clear();
			_key = key;
			_shm = xopen(mode);
			_key = getkey();
			_addr = xattach();
			if (_owner) {
				xfill();
			}
			_size = xsize();
		}

		void
		do_open(path_type&& name, mode_type mode, char num)
		{
			_path = this->make_path(std::forward<path_type>(name));
			_key = xgenkey(num);
			_shm = xopen(mode);
			_addr = xattach();
			if (_owner) {
				xfill();
			}
			_size = xsize();
		}

		key_type
		getkey() const {
			struct ::shmid_ds stat;
			bits::check(::shmctl(_shm, IPC_STAT, &stat),
				__FILE__, __LINE__, __func__);
			return stat.shm_perm.__key;
		}

		void
		close() {
			xdetach();
			xclose();
			xunlink();
		}

		void
		xfill() noexcept {
			std::uninitialized_fill(this->begin(),
				this->end(), value_type());
		}

		key_type
		xgenkey(char num) const {
			{ std::filebuf f; f.open(_path, std::ios_base::in); }
			return bits::check(::ftok(_path.c_str(), num),
				__FILE__, __LINE__, __func__);
		}

		shm_type
		xopen(int flags=0) const {
			return bits::check(::shmget(_key,
				size()*sizeof(value_type), flags),
				__FILE__, __LINE__, __func__);
		}

		addr_type
		xattach() const {
			return bits::check(::shmat(_shm, 0, 0),
				__FILE__, __LINE__, __func__);
		}

		void
		xdetach() {
			if (_addr) {
				bits::check(::shmdt(_addr),
					__FILE__, __LINE__, __func__);
				_addr = nullptr;
			}
		}

		void
		xclose() const {
			if (_owner) {
				bits::check(::shmctl(_shm, IPC_RMID, 0),
					__FILE__, __LINE__, __func__);
			}
		}

		void
		xunlink() const {
			if (_owner && !_path.empty()) {
				bits::check(::unlink(_path.c_str()),
					__FILE__, __LINE__, __func__);
			}
		}

		size_type
		xsize() const {
			::shmid_ds stat;
			bits::check(::shmctl(_shm, IPC_STAT, &stat),
				__FILE__, __LINE__, __func__);
			return stat.shm_segsz / sizeof(value_type);
		}

		mode_type
		xmode() const {
			::shmid_ds stat;
			bits::check(::shmctl(_shm, IPC_STAT, &stat),
				__FILE__, __LINE__, __func__);
			return stat.shm_perm.mode;
		}

		static inline path_type
		make_path(path_type&& rhs) {
			return path_type("/tmp" + std::forward<path_type>(rhs));
		}

		friend std::ostream&
		operator<<(std::ostream& out, const shared_mem& rhs) {
			return out << "{addr=" << rhs.ptr()
				<< ",path=" << rhs._path
				<< ",size=" << rhs.size()
				<< ",owner=" << rhs.is_owner()
				<< ",key=" << rhs._key
				<< ",shm=" << rhs._shm
				<< '}';
		}

		std::string _path;
		size_type _size = 0;
		key_type _key = 0;
		shm_type _shm = 0;
		addr_type _addr = nullptr;
		bool _owner = false;

		constexpr static const char NO_PROJ_ID = 'a';
	};

}

#endif // SYSX_SHAREDMEM_HH
