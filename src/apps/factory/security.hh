#ifndef APPS_FACTORY_SECURITY_HH
#define APPS_FACTORY_SECURITY_HH

#include <sys/time.h>
#include <sys/resource.h>
#if defined(__APPLE__)
	#include <crt_externs.h>
	#define environ (*_NSGetEnviron())
#endif

#include <iostream>
#include <cmath>
#include <vector>
#include <iterator>
#include <algorithm>
#include <cstring>

#include <unistdx/base/check>
#include <unistdx/io/poller>
#include <unistdx/ipc/identity>
#include <unistdx/ipc/process>
#include <unistdx/it/cstring_iterator>

namespace factory {

	namespace bits {

		template<class F>
		void
		for_each_open_file_descriptor(F func) {
			struct ::rlimit rlim;
			UNISTDX_CHECK(::getrlimit(RLIMIT_NOFILE, &rlim));
			const int num_fds = rlim.rlim_cur == RLIM_INFINITY ? FD_SETSIZE : rlim.rlim_cur;
			const int batch_size = FD_SETSIZE;
			const int num_batches = num_fds / batch_size + (num_fds % FD_SETSIZE == 0 ? 0 : 1);
			const int no_timeout = -1;
			for (int i=0; i<num_batches; ++i) {
				std::vector<sys::poll_event> fds;
				const int from = i*batch_size;
				const int to = std::min((i+1)*batch_size, num_fds);
				for (int fd=from; fd<to; ++fd) {
					fds.emplace_back(fd, 0, 0);
				}
				UNISTDX_CHECK(::poll(fds.data(), fds.size(), no_timeout));
				std::for_each(
					fds.begin(), fds.end(),
					[&func] (const sys::poll_event& rhs) {
						if (not rhs.bad()) {
							func(rhs);
						}
					}
				);
			}

		}

		template<class F>
		void
		for_each_env(F func) {
			std::for_each(
				sys::cstring_iterator<char*>(environ),
				sys::cstring_iterator<char*>(),
				func
			);
		}

	}

	void
	do_not_allow_ld_preload() {
		const char* bad_prefix = "LD_";
		bits::for_each_env(
			[bad_prefix] (char* rhs) {
				if (std::strstr(rhs, bad_prefix)) {
					throw std::runtime_error("LD_* environment variable detected");
				};
			}
		);
	}

	void
	shred_environment() {
		bits::for_each_env(
			[] (char* rhs) {
				std::memset(rhs, 0, std::strlen(rhs));
			}
		);
	}

	void
	do_not_allow_to_run_as_superuser_or_setuid() {
		using namespace sys::this_process;
		if (user() == sys::superuser() or effective_user() == sys::superuser() or
			group() == sys::supergroup() or effective_group() == sys::supergroup()) {
			throw std::runtime_error("do not run as superuser/supergroup");
		}
		if (user() != effective_user() or
			group() != effective_group()) {
			throw std::runtime_error("do not run as setuid/setgid");
		}
	}

	void
	ignore_all_signals_except_terminate() {
		for (sys::signal_type s=1; s<=31; ++s) {
			sys::signal sig = sys::signal(s);
			if (sig != sys::signal::terminate and
				sig != sys::signal::kill and
				sig != sys::signal::stop)
			{
				sys::this_process::ignore_signal(sig);
			}
		}
	}

	void
	close_all_file_descriptors() {
		bits::for_each_open_file_descriptor(
			[] (const sys::poll_event& rhs) {
				#ifdef NDEBUG
				sys::fildes tmp(rhs.fd());
				#else
				if (rhs.fd() > 2) { sys::fildes tmp(rhs.fd()); }
				#endif
				// destructor closes file descriptor
			}
		);
	}

}

#endif // vim:filetype=cpp
