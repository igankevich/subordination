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

	struct fildes {

		enum flag: flag_type {
			non_blocking = O_NONBLOCK,
			append = O_APPEND,
			async = O_ASYNC,
			dsync = O_DSYNC,
			sync = O_SYNC,
			create = O_CREAT,
			truncate = O_TRUNC
		};

		enum fd_flag: flag_type {
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

		explicit
		fildes(fd_type rhs) noexcept:
			_fd(rhs) {}

		fildes(fildes&& rhs) noexcept: _fd(rhs._fd) {
			rhs._fd = bad;
		}

		~fildes() {
			this->close();
		}

		fildes&
		operator=(fildes&& rhs) {
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

		ssize_t
		read(void* buf, size_t n) const noexcept {
			return ::read(this->_fd, buf, n);
		}

		ssize_t
		write(const void* buf, size_t n) const noexcept {
			return ::write(this->_fd, buf, n);
		}

		fd_type
		get_fd() const noexcept {
			return this->_fd;
		}

		flag_type
		flags() const {
			return get_flags(F_GETFL);
		}

		flag_type
		fd_flags() const {
			return get_flags(F_GETFD);
		}

		void
		setf(flag_type rhs) {
			set_flag(F_SETFL, get_flags(F_GETFL) | rhs);
		}

		#ifdef F_SETNOSIGPIPE
		void
		setf(pipe_flag rhs) {
			set_flag(F_SETNOSIGPIPE, 1);
		}

		void
		unsetf(pipe_flag rhs) {
			set_flag(F_SETNOSIGPIPE, 0);
		}
		#endif

		void
		setf(fd_flag rhs) {
			set_flag(F_SETFD, rhs);
		}

		bool
		operator==(const fildes& rhs) const noexcept {
			return this->_fd == rhs._fd;
		}

		explicit
		operator bool() const noexcept {
			return this->_fd >= 0;
		}

		bool
		operator !() const noexcept {
			return !operator bool();
		}

		bool
		operator==(fd_type rhs) const noexcept {
			return _fd == rhs;
		}

		friend bool
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
