#ifndef APPS_FACTORY_SECURITY_HH
#define APPS_FACTORY_SECURITY_HH

#include <sys/time.h>
#include <sys/resource.h>

#include <iostream>
#include <cmath>
#include <vector>
#include <iterator>
#include <algorithm>

#include <stdx/iterator.hh>

#include <sysx/bits/check.hh>
#include <sysx/event.hh>
#include <sysx/process.hh>

namespace factory {

	namespace bits {

		template<class F>
		void
		for_each_open_file_descriptor(F func) {
			struct ::rlimit rlim;
			sysx::bits::check(::getrlimit(RLIMIT_NOFILE, &rlim), __FILE__, __LINE__, __func__);
			const int num_fds = rlim.rlim_cur == RLIM_INFINITY ? FD_SETSIZE : rlim.rlim_cur;
			const int batch_size = FD_SETSIZE;
			const int num_batches = num_fds / batch_size + (num_fds % FD_SETSIZE == 0 ? 0 : 1);
			const int no_timeout = -1;
			for (int i=0; i<num_batches; ++i) {
				std::vector<sysx::poll_event> fds;
				const int from = i*batch_size;
				const int to = std::min((i+1)*batch_size, num_fds);
				for (int fd=from; fd<to; ++fd) {
					fds.emplace_back(fd, 0, 0);
				}
				sysx::bits::check(::poll(fds.data(), fds.size(), no_timeout), __FILE__, __LINE__, __func__);
				std::for_each(
					fds.begin(), fds.end(),
					[&func] (const sysx::poll_event& rhs) {
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
			std::for_each(std::begin(environ), std::end(environ), func);
		}

	}

	void
	do_not_allow_ld_preload() {
		const char* bad_prefix = "LD_";
		bits::for_each_env(
			[bad_prefix] (char* rhs) {
				if (std::begin(rhs) == std::search(
					std::begin(rhs), std::end(rhs),
					std::begin(bad_prefix), std::end(bad_prefix)
				)) {
					throw std::runtime_error("LD_* environment variable detected");
				};
			}
		);
	}

	void
	shred_environment() {
		bits::for_each_env(
			[] (char* rhs) {
				std::fill(std::begin(rhs), std::end(rhs), '\0');
			}
		);
	}

	void
	do_not_allow_to_run_as_superuser_or_setuid() {
		using namespace sysx::this_process;
		if (user_id() == 0 or effective_user_id() == 0 or
			group_id() == 0 or effective_group_id() == 0) {
			throw std::runtime_error("do not run as superuser/supergroup");
		}
		if (user_id() != effective_user_id() or
			group_id() != effective_group_id()) {
			throw std::runtime_error("do not run as setuid/setgid");
		}
	}

	void
	ignore_all_signals_except_terminate() {
		for (sysx::signal_type s=1; s<=31; ++s) {
			sysx::signal sig = sysx::signal(s);
			if (sig != sysx::signal::terminate and
				sig != sysx::signal::kill and
				sig != sysx::signal::stop)
			{
				sysx::this_process::ignore_signal(sig);
			}
		}
	}

	void
	close_all_file_descriptors() {
		bits::for_each_open_file_descriptor(
			[] (const sysx::poll_event& rhs) {
				#ifdef NDEBUG
				sysx::fildes tmp(rhs.fd());
				#else
				if (rhs.fd() > 2) { sysx::fildes tmp(rhs.fd()); }
				#endif
				// destructor closes file descriptor
			}
		);
	}

}

#endif // APPS_FACTORY_SECURITY_HH
