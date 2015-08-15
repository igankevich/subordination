namespace factory {

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

	}

}
