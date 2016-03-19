#ifndef SYSX_SOCKET_HH
#define SYSX_SOCKET_HH

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#if !defined(SOCK_NONBLOCK)
	#define SOCK_NONBLOCK 0
#endif
#if !defined(SOCK_CLOEXEC)
	#define SOCK_CLOEXEC 0
#endif

#include <stdx/log.hh>

#include <sysx/fildes.hh>
#include <sysx/endpoint.hh>


namespace sysx {

	namespace bits {

		int
		safe_socket(int domain, int type, int protocol) {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			int sock = check(
				::socket(domain, type, protocol),
				__FILE__, __LINE__, __func__);
			#if !SOCK_NONBLOCK || !SOCK_CLOEXEC
			set_mandatory_flags(sock);
			#endif
			return sock;
		}

	}

	struct socket: public fildes {

		typedef int opt_type;
		typedef stdx::log<socket> this_log;

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
			sysx::fildes(std::move(rhs)) {}

		explicit
		socket(fildes&& rhs) noexcept:
			sysx::fildes(std::move(rhs)) {}

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
			sysx::fildes::operator=(std::move(static_cast<sysx::fildes&&>(rhs)));
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
			this_log() << "Binding to " << e << std::endl;
			bits::check(::bind(this->_fd, e.sockaddr(), e.sockaddrlen()),
				__FILE__, __LINE__, __func__);
		}

		void listen() {
			this_log() << "Listening on " << this->name() << std::endl;
			bits::check(::listen(this->_fd, SOMAXCONN),
				__FILE__, __LINE__, __func__);
		}

		void connect(const endpoint& e) {
			this->create_socket_if_necessary();
			this_log() << "Connecting to " << e << std::endl;
			bits::check_if_not<std::errc::operation_in_progress>(
				::connect(this->_fd, e.sockaddr(), e.sockaddrlen()),
				__FILE__, __LINE__, __func__);
		}

		void accept(socket& sock, endpoint& addr) {
			socklen_type len = sizeof(endpoint);
			sock.close();
			sock._fd = bits::check(::accept(this->_fd, addr.sockaddr(), &len),
				__FILE__, __LINE__, __func__);
			bits::set_mandatory_flags(sock._fd);
			this_log() << "Accepted connection from " << addr << std::endl;
		}

		void shutdown(shutdown_how how) {
			if (*this) {
				bits::check_if_not<std::errc::not_connected>(
					::shutdown(this->_fd, how),
					__FILE__, __LINE__, __func__);
			}
		}

		void close() {
			this->shutdown(shut_read_write);
			this->sysx::fildes::close();
		}

		void setopt(option opt) {
			int one = 1;
			bits::check(::setsockopt(this->_fd,
				SOL_SOCKET, opt, &one, sizeof(one)),
				__FILE__, __LINE__, __func__);
		}

		/// @deprecated We use event-based error notifications.
		int error() const {
			int ret = 0;
			int opt = 0;
			if (!*this) {
				ret = -1;
			} else {
				socklen_type sz = sizeof(opt);
				bits::check_if_not<std::errc::not_a_socket>(::getsockopt(this->_fd, SOL_SOCKET, SO_ERROR, &opt, &sz),
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

		endpoint
		bind_addr() const {
			endpoint addr;
			socklen_type len = sizeof(endpoint);
			int ret = ::getsockname(this->_fd, addr.sockaddr(), &len);
			return ret == -1 ? endpoint() : addr;
		}

		endpoint
		name() const {
			endpoint addr;
			socklen_type len = sizeof(endpoint);
			bits::check(::getsockname(this->_fd, addr.sockaddr(), &len),
				__FILE__, __LINE__, __func__);
			return addr;
		}

		endpoint peer_name() const {
			endpoint addr;
			socklen_type len = sizeof(endpoint);
			bits::check(::getpeername(this->_fd, addr.sockaddr(), &len),
				__FILE__, __LINE__, __func__);
			return addr;
		}

		uint32_t read(void* buf, size_t size) {
			ssize_t ret = ::read(this->_fd, buf, size);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		uint32_t write(const void* buf, size_t size) {
			ssize_t ret = ::write(this->_fd, buf, size);
			return ret == -1 ? 0 : static_cast<uint32_t>(ret);
		}

		fd_type fd() const noexcept { return this->_fd; }

		friend std::ostream& operator<<(std::ostream& out, const socket& rhs) {
			return out << "{fd=" << rhs._fd << ",st="
				<< (rhs.error() == 0 ? "ok" : std::error_code(errno, std::generic_category()).message())
				<< '}';
		}

	protected:

		explicit
		socket(fd_type sock) noexcept:
			sysx::fildes(sock) {}

	};

}

#endif // SYSX_SOCKET_HH
