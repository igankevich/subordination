#ifndef FACTORY_UNISTDX_FILDES_HH
#define FACTORY_UNISTDX_FILDES_HH

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(SOCK_NONBLOCK)
	#define SOCK_NONBLOCK 0
#endif
#if !defined(SOCK_CLOEXEC)
	#define SOCK_CLOEXEC 0
#endif

namespace factory {

	namespace bits {

		int
		safe_socket(int domain, int type, int protocol) {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			int sock = unix::check(
				::socket(domain, type, protocol),
				__FILE__, __LINE__, __func__);
			#if !SOCK_NONBLOCK || !SOCK_CLOEXEC
			set_mandatory_flags(sock);
			#endif
			return sock;
		}

	}

	namespace unix {

		struct fd_flag;

		struct fd {

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

			enum pipe_flag: flag_type {
				no_sigpipe = 1
			};

			static const fd_type
			bad = -1;

			fd() = default;
			fd(const fd&) = delete;
			fd& operator=(const fd&) = delete;

			inline explicit
			fd(fd_type rhs) noexcept:
				_fd(rhs) {}

			inline
			fd(fd&& rhs) noexcept: _fd(rhs._fd) {
				rhs._fd = bad;
			}

			~fd() {
				this->close();
			}

			inline
			fd& operator=(fd&& rhs) {
				_fd = rhs._fd;
				rhs._fd = bad;
				return *this;
			}
			
			void close() {
				using components::check;
				if (*this) {
					check(::close(this->_fd),
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

			inline void
			setf(pipe_flag rhs) {
				set_flag(F_SETNOSIGPIPE, 1);
			}

			inline void
			unsetf(pipe_flag rhs) {
				set_flag(F_SETNOSIGPIPE, 0);
			}

			inline void
			set_flags(flag_type rhs) {
				set_flag(F_SETFD, rhs);
			}

			inline void setf(fd_flag fls);

			inline bool
			operator==(const fd& rhs) const noexcept {
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
			operator==(fd_type lhs, const fd& rhs) noexcept {
				return rhs._fd == lhs;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const fd& rhs) {
				return out << "{fd=" << rhs._fd << '}';
			}

		private:

			flag_type
			get_flags(int which) const {
				using components::check;
				return check(::fcntl(this->_fd, which),
					__FILE__, __LINE__, __func__);
			}

			void
			set_flag(int which, flag_type val) {
				using components::check;
				check(::fcntl(this->_fd, which, val),
					__FILE__, __LINE__, __func__);
			}

		protected:
			fd_type _fd = bad;
		};

		static_assert(sizeof(fd) == sizeof(fd_type), "bad fd size");

		struct fd_flag {

			constexpr
			fd_flag(fd::status_flag x, fd::flag y) noexcept:
				f1(x), f2(y) {}

			constexpr
			fd_flag(fd::flag x, fd::status_flag y) noexcept:
				f1(y), f2(x) {}

			constexpr
			fd_flag(const fd_flag&) noexcept = default;

			constexpr
			fd_flag() noexcept = default;

			void set(fd& f) const {
				f.setf(f1);
				f.set_flags(f2);
			}

			static inline fd_flag
			flags(const fd& f) {
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
		operator|(fd::status_flag lhs, fd::flag rhs) noexcept {
			return fd_flag(lhs, rhs);
		}

		constexpr fd_flag
		operator|(fd::flag lhs, fd::status_flag rhs) noexcept {
			return fd_flag(lhs, rhs);
		}


		fd_flag
		fd::flags() const {
			return fd_flag::flags(*this);
		}

		void
		fd::setf(fd_flag f) {
			f.set(*this);
		}

		struct file: public unix::fd {

			enum openmode {
				read_only=O_RDONLY,
				write_only=O_WRONLY,
				read_write=O_RDWR
			};

			file() noexcept = default;

			explicit
			file(file&& rhs) noexcept:
				unix::fd(std::move(rhs)) {}

			explicit
			file(const std::string& filename, openmode flags,
				unix::flag_type flags2=0, mode_type mode=S_IRUSR|S_IWUSR):
				unix::fd(components::check(bits::safe_open(filename.c_str(), flags|flags2, mode),
					__FILE__, __LINE__, __func__)) {}

			~file() {
				this->close();
			}

		};

		struct socket: public fd {

			typedef int opt_type;

			enum option: opt_type {
				reuse_addr = SO_REUSEADDR,
				keep_alive = SO_KEEPALIVE
			};

			enum shutdown_how {
				shut_read = SHUT_RD,
				shut_write = SHUT_WR,
				shut_read_write = SHUT_RDWR
			};

			static const flag_type
			DEFAULT_FLAGS = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;

			socket() = default;
			socket(const socket&) = delete;
			socket& operator=(const socket&) = delete;

			explicit
			socket(socket&& rhs) noexcept:
				unix::fd(std::move(rhs)) {}

			/// Bind on @bind_addr and listen.
			explicit
			socket(const endpoint& bind_addr) {
				this->bind(bind_addr);
				this->listen();
			}

			/// Bind on @bind_addr and connect to a server on @conn_addr.
			explicit
			socket(const endpoint& bind_addr, const endpoint& conn_addr) {
				this->bind(bind_addr);
				this->connect(conn_addr);
			}

			~socket() {
				this->close();
			}

			socket&
			operator=(socket&& rhs) {
				unix::fd::operator=(std::move(static_cast<unix::fd&&>(rhs)));
				return *this;
			}

			void create_socket_if_necessary() {
				if (!*this) {
					this->_fd = bits::safe_socket(AF_INET, DEFAULT_FLAGS, 0);
				}
			}

			void bind(const endpoint& e) {
				this->create_socket_if_necessary();
				this->setopt(reuse_addr);
				std::clog << "Binding to " << e << std::endl;
				check(::bind(this->_fd, e.sockaddr(), e.sockaddrlen()),
					__FILE__, __LINE__, __func__);
			}
			
			void listen() {
				std::clog << "Listening on " << this->name() << std::endl;
				check(::listen(this->_fd, SOMAXCONN),
					__FILE__, __LINE__, __func__);
			}

			void connect(const endpoint& e) {
				this->create_socket_if_necessary();
				std::clog << "Connecting to " << e << std::endl;
				check_if_not<std::errc::operation_in_progress>(
					::connect(this->_fd, e.sockaddr(), e.sockaddrlen()),
					__FILE__, __LINE__, __func__);
			}

			void accept(socket& sock, endpoint& addr) {
				socklen_type len = sizeof(endpoint);
				sock.close();
				sock._fd = check(::accept(this->_fd, addr.sockaddr(), &len),
					__FILE__, __LINE__, __func__);
				sock.setf(unix::fd::non_blocking | unix::fd::close_on_exec);
//				sock.setf(close_on_exec);
				std::clog << "Accepted connection from " << addr << std::endl;
			}

			void shutdown(shutdown_how how) {
				if (*this) {
					std::clog
						<< "Closing socket "
						<< this->_fd << std::endl;
					check_if_not<std::errc::not_connected>(
						::shutdown(this->_fd, how),
						__FILE__, __LINE__, __func__);
				}
			}

			void close() {
				this->shutdown(shut_read_write);
				this->unix::fd::close();
			}

			void setopt(option opt) {
				int one = 1;
				check(::setsockopt(this->_fd,
					SOL_SOCKET, opt, &one, sizeof(one)),
					__FILE__, __LINE__, __func__);
			}

			int error() const {
				int ret = 0;
				int opt = 0;
				if (!*this) {
					ret = -1;
				} else {
					socklen_type sz = sizeof(opt);
					check_if_not<std::errc::not_a_socket>(::getsockopt(this->_fd, SOL_SOCKET, SO_ERROR, &opt, &sz),
						__FILE__, __LINE__, __func__);
				}
				// ignore EAGAIN since it is common 'error' in asynchronous programming
				if (opt == EAGAIN || opt == EINPROGRESS) {
					ret = 0;
				} else {
					// If one connects to localhost to a different port and the service is offline
					// then socket's local port can be chosen to be the same as the port of the service.
					// If this happens the socket connects to itself and sends and replies to
					// its own messages (at least on Linux). This conditional solves the issue.
					try {
						if (ret == 0 && this->name() == this->peer_name()) {
							ret = -1;
						}
					} catch (...) {
						ret = -1;
					}
				}
				return ret;
			}

			endpoint bind_addr() const {
				endpoint addr;
				socklen_type len = sizeof(endpoint);
				int ret = ::getsockname(this->_fd, addr.sockaddr(), &len);
				return ret == -1 ? endpoint() : addr;
			}

			endpoint name() const {
				endpoint addr;
				socklen_type len = sizeof(endpoint);
				check(::getsockname(this->_fd, addr.sockaddr(), &len),
					__FILE__, __LINE__, __func__);
				return addr;
			}

			endpoint peer_name() const {
				endpoint addr;
				socklen_type len = sizeof(endpoint);
				check(::getpeername(this->_fd, addr.sockaddr(), &len),
					__FILE__, __LINE__, __func__);
				return addr;
			}

			uint32_t read(void* buf, size_t size) {
				ssize_t ret = ::read(this->_fd, buf, size);
				return ret == -1 ? 0 : static_cast<uint32_t>(ret);
			}

			uint32_t write(const void* buf, size_t size) {
				ssize_t ret = ::write(this->_fd, buf, size);
//				ssize_t ret = ::send(_fd, buf, size, MSG_NOSIGNAL);
				return ret == -1 ? 0 : static_cast<uint32_t>(ret);
			}

			fd_type fd() const noexcept { return this->_fd; }

			friend std::ostream& operator<<(std::ostream& out, const socket& rhs) {
				return out << "{fd=" << rhs._fd << ",st="
					<< (rhs.error() == 0 ? "ok" : std::error_code(errno, std::generic_category()).message())
					<< '}';
			}

			// TODO: remove this ``boilerplate''
			bool empty() const { return true; }
			bool flush() const { return true; }

		protected:

			explicit
			socket(fd_type sock) noexcept:
				unix::fd(sock) {}

		};

		union pipe {

			inline
			pipe(): _fds{} {
				open();
			}

			inline
			pipe(pipe&& rhs) noexcept:
				_fds{std::move(rhs._fds[0]), std::move(rhs._fds[1])}
			{}

			inline
			~pipe() {}

			inline fd&
			in() noexcept {
				return this->_fds[0];
			}

			inline fd&
			out() noexcept {
				return this->_fds[1];
			}

			inline const fd&
			in() const noexcept {
				return this->_fds[0];
			}

			inline const fd&
			out() const noexcept {
				return this->_fds[1];
			}
			
			void open() {
				this->close();
				using components::check;
				check(bits::safe_pipe(this->_rawfds),
					__FILE__, __LINE__, __func__);
			}

			void close() {
				in().close();
				out().close();
			}

		private:
			unix::fd _fds[2] = {};
			fd_type _rawfds[2];

			static_assert(sizeof(_fds) == sizeof(_rawfds), "bad unix::fd size");
		};

	}

}
#endif // FACTORY_UNISTDX_FILDES_HH
