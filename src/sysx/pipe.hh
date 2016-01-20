#ifndef SYSX_PIPE_HH
#define SYSX_PIPE_HH

#include <unistd.h>

#include <sysx/bits/check.hh>
#include <sysx/bits/safe_calls.hh>
#include <sysx/fildes.hh>

namespace sysx {

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

		inline fildes&
		in() noexcept {
			return this->_fds[0];
		}

		inline fildes&
		out() noexcept {
			return this->_fds[1];
		}

		inline const fildes&
		in() const noexcept {
			return this->_fds[0];
		}

		inline const fildes&
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
		sysx::fildes _fds[2] = {};
		fd_type _rawfds[2];

		static_assert(sizeof(_fds) == sizeof(_rawfds), "bad sysx::fildes size");
	};

	struct two_way_pipe {

		two_way_pipe():
		_owner(sysx::this_process::id())
		{}
		~two_way_pipe() = default;
		two_way_pipe(const two_way_pipe&) = delete;
		two_way_pipe(two_way_pipe&&) = default;
		two_way_pipe& operator=(two_way_pipe&) = delete;

		fildes& parent_in() noexcept { return _pipe1.in(); }
		fildes& parent_out() noexcept { return _pipe2.out(); }
		const fildes& parent_in() const noexcept { return _pipe1.in(); }
		const fildes& parent_out() const noexcept { return _pipe2.out(); }

		fildes& child_in() noexcept { return _pipe2.in(); }
		fildes& child_out() noexcept { return _pipe1.out(); }
		const fildes& child_in() const noexcept { return _pipe2.in(); }
		const fildes& child_out() const noexcept { return _pipe1.out(); }

		void open() {
			_pipe1.open();
			_pipe2.open();
		}

		void close() {
			_pipe1.close();
			_pipe2.close();
		}

		void close_in_child() {
			_pipe1.in().close();
			_pipe2.out().close();
		}

		void close_in_parent() {
			_pipe1.out().close();
			_pipe2.in().close();
		}

		void remap_in_child(fd_type in, fd_type out) {
			child_in().remap(in);
			child_out().remap(out);
		}

		bool
		is_owner() const {
			return sysx::this_process::id() == _owner;
		}

		void
		close_unused() {
			is_owner() ? close_in_parent() : close_in_child();
		}

	private:

		pipe _pipe1;
		pipe _pipe2;
		pid_type _owner;

	};

}

#endif // SYSX_PIPE_HH
