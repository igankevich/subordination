namespace factory {

	namespace unix {

		union pipe {

			inline
			pipe(): _fds{} {
				open();
			}

			inline
			pipe(fd_flag in_flags, fd_flag out_flags=fd_flag()): _fds{} {
				open();
				if (in_flags) {
					in_flags.set(in());
				}
				if (out_flags) {
					out_flags.set(out());
				}
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
				check(::pipe(this->_rawfds),
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
