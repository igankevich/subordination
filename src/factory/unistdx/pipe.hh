#ifndef FACTORY_UNISTDX_PIPE_HH
#define FACTORY_UNISTDX_PIPE_HH

#include <unistd.h>

#include "../bits/check.hh"
#include "../bits/safe_calls.hh"
#include "fildes.hh"

namespace factory {

	namespace unix {

		fd_type
		safe_pipe(fd_type fds[2]) {
			bits::global_lock_type lock(bits::__forkmutex);
			int ret = ::pipe(fds);
			if (ret != -1) {
				bits::set_mandatory_flags(fds[0]);
				bits::set_mandatory_flags(fds[1]);
				#if defined(F_SETNOSIGPIPE)
				fcntl(fds[1], F_SETNOSIGPIPE, 1);
				#endif
			}
			return ret;
		}

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
				bits::check(safe_pipe(this->_rawfds),
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

#endif // FACTORY_UNISTDX_PIPE_HH
