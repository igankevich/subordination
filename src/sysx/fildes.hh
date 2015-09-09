#ifndef SYSX_FILDES_HH
#define SYSX_FILDES_HH

#include <unistd.h>
#include <fcntl.h>

#include <sysx/bits/check.hh>
#include <sysx/bits/safe_calls.hh>

namespace sysx {

	typedef ::mode_t mode_type;
	typedef int fd_type;
	typedef int flag_type;

	fd_type
	safe_open(const char* path, flag_type oflag, mode_type mode) {
		bits::global_lock_type lock(bits::__forkmutex);
		fd_type ret = ::open(path, oflag, mode);
		if (ret != -1) {
			bits::set_mandatory_flags(ret);
		}
		return ret;
	}

	struct fd_flag;

	struct fildes {

		enum status_flag: flag_type {
			non_blocking = O_NONBLOCK,
			append = O_APPEND,
			async = O_ASYNC,
			dsync = O_DSYNC,
			sync = O_SYNC,
			create = O_CREAT,
			truncate = O_TRUNC
		};

		enum flag: flag_type {
			close_on_exec = FD_CLOEXEC
		};

		#ifdef F_SETNOSIGPIPE
		enum pipe_flag: flag_type {
			no_sigpipe = 1
		};
		#endif

		static const fd_type
		bad = -1;

		fildes() = default;
		fildes(const fildes&) = delete;
		fildes& operator=(const fildes&) = delete;

		inline explicit
		fildes(fd_type rhs) noexcept:
			_fd(rhs) {}

		inline
		fildes(fildes&& rhs) noexcept: _fd(rhs._fd) {
			rhs._fd = bad;
		}

		~fildes() {
			this->close();
		}

		inline
		fildes& operator=(fildes&& rhs) {
			_fd = rhs._fd;
			rhs._fd = bad;
			return *this;
		}
		
		void close() {
			if (*this) {
				bits::check(::close(this->_fd),
					__FILE__, __LINE__, __func__);
				this->_fd = bad;
			}
		}

		inline ssize_t
		read(void* buf, size_t n) const noexcept {
			return ::read(this->_fd, buf, n);
		}

		inline ssize_t
		write(const void* buf, size_t n) const noexcept {
			return ::write(this->_fd, buf, n);
		}

		inline fd_type
		get_fd() const noexcept {
			return this->_fd;
		}

		inline flag_type
		status_flags() const {
			return get_flags(F_GETFL);
		}

		inline flag_type
		fd_flags() const {
			return get_flags(F_GETFD);
		}

		inline fd_flag flags() const;

		inline void
		setf(flag_type rhs) {
			set_flag(F_SETFL, get_flags(F_GETFL) | rhs);
		}

		#ifdef F_SETNOSIGPIPE
		inline void
		setf(pipe_flag rhs) {
			set_flag(F_SETNOSIGPIPE, 1);
		}

		inline void
		unsetf(pipe_flag rhs) {
			set_flag(F_SETNOSIGPIPE, 0);
		}
		#endif

		inline void
		set_flags(flag_type rhs) {
			set_flag(F_SETFD, rhs);
		}

		inline void setf(fd_flag fls);

		inline bool
		operator==(const fildes& rhs) const noexcept {
			return this->_fd == rhs._fd;
		}

		inline explicit
		operator bool() const noexcept {
			return this->_fd >= 0;
		}

		inline bool
		operator !() const noexcept {
			return !operator bool();
		}

		inline bool
		operator==(fd_type rhs) const noexcept {
			return _fd == rhs;
		}

		friend inline bool
		operator==(fd_type lhs, const fildes& rhs) noexcept {
			return rhs._fd == lhs;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const fildes& rhs) {
			return out << "{fd=" << rhs._fd << '}';
		}

	private:

		flag_type
		get_flags(int which) const {
			return bits::check(::fcntl(this->_fd, which),
				__FILE__, __LINE__, __func__);
		}

		void
		set_flag(int which, flag_type val) {
			bits::check(::fcntl(this->_fd, which, val),
				__FILE__, __LINE__, __func__);
		}

	protected:
		fd_type _fd = bad;
	};

	static_assert(sizeof(fildes) == sizeof(fd_type), "bad fd size");

	struct fd_flag {

		constexpr
		fd_flag(fildes::status_flag x, fildes::flag y) noexcept:
			f1(x), f2(y) {}

		constexpr
		fd_flag(fildes::flag x, fildes::status_flag y) noexcept:
			f1(y), f2(x) {}

		constexpr
		fd_flag(const fd_flag&) noexcept = default;

		constexpr
		fd_flag() noexcept = default;

		void set(fildes& f) const {
			f.setf(f1);
			f.set_flags(f2);
		}

		static inline fd_flag
		flags(const fildes& f) {
			return fd_flag(f.status_flags(), f.fd_flags());
		}

		friend constexpr fd_flag
		operator|(fd_flag lhs, fd_flag rhs) noexcept {
			return fd_flag(lhs.f1 | rhs.f1, lhs.f2 | rhs.f2);
		}

		friend constexpr fd_flag
		operator&(fd_flag lhs, fd_flag rhs) noexcept {
			return fd_flag(lhs.f1 & rhs.f1, lhs.f2 & rhs.f2);
		}

		explicit constexpr
		operator bool() const noexcept {
			return f1 != 0 || f2 != 0;
		}

		constexpr bool
		operator !() const noexcept {
			return !operator bool();
		}

		constexpr fd_flag
		operator~() const noexcept {
			return fd_flag(~f1, ~f2);
		}

		constexpr bool
		operator==(const fd_flag& rhs) const noexcept {
			return f1 == rhs.f1 && f2 == rhs.f2;
		}

		constexpr bool
		operator!=(const fd_flag& rhs) const noexcept {
			return !operator==(rhs);
		}

	private:

		constexpr
		fd_flag(flag_type x, flag_type y) noexcept:
			f1(x), f2(y) {}

		flag_type f1 = 0;
		flag_type f2 = 0;
	};

	constexpr fd_flag
	operator|(fildes::status_flag lhs, fildes::flag rhs) noexcept {
		return fd_flag(lhs, rhs);
	}

	constexpr fd_flag
	operator|(fildes::flag lhs, fildes::status_flag rhs) noexcept {
		return fd_flag(lhs, rhs);
	}


	fd_flag
	fildes::flags() const {
		return fd_flag::flags(*this);
	}

	void
	fildes::setf(fd_flag f) {
		f.set(*this);
	}

	struct file: public sysx::fildes {

		enum openmode {
			read_only=O_RDONLY,
			write_only=O_WRONLY,
			read_write=O_RDWR
		};

		file() noexcept = default;

		explicit
		file(file&& rhs) noexcept:
			sysx::fildes(std::move(rhs)) {}

		explicit
		file(const std::string& filename, openmode flags,
			sysx::flag_type flags2=0, mode_type mode=S_IRUSR|S_IWUSR):
			sysx::fildes(bits::check(safe_open(filename.c_str(), flags|flags2, mode),
				__FILE__, __LINE__, __func__)) {}

		~file() {
			this->close();
		}

	};

	struct tmpfile: public file {

		typedef std::string path_type;

		tmpfile() noexcept = default;

		explicit
		tmpfile(tmpfile&& rhs) noexcept:
		sysx::file(std::move(rhs)),
		_path(std::move(rhs._path))
		{}

		explicit
		tmpfile(const path_type& filename, openmode flags=read_write,
		sysx::flag_type flags2=create|truncate, mode_type mode=S_IRUSR|S_IWUSR):
		file(filename, flags, flags2, mode),
		_path(filename)
		{}

		~tmpfile() {
			this->close();
			this->unlink();
		}

	private:

		void
		unlink() noexcept {
			::unlink(_path.c_str());
		}

		path_type _path;
	};

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_fildesbuf: public std::basic_streambuf<Ch,Tr> {

		typedef std::basic_streambuf<Ch,Tr> base_type;
		using typename base_type::char_type;

		std::streamsize
		xsputn(const char_type* s, std::streamsize n) override {
			return _fd.write(s, n);
		}

		std::streamsize
		xsgetn(char_type* s, std::streamsize n) override {
			return _fd.read(s, n);
		}

	private:
		sysx::fildes& _fd;
	};

}

#endif // SYSX_FILDES_HH
