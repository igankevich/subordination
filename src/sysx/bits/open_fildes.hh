#include <sys/time.h>
#include <sys/resource.h>

#include <cmath>
#include <algorithm>
#include <vector>
#include <iterator>

#include <sysx/event.hh>
#include <sysx/bits/check.hh>

namespace sysx {
	namespace bits {

		void
		list_open_file_descriptors() {
			struct ::rlimit rlim;
			check(::getrlimit(RLIMIT_NOFILE, &rlim), __FILE__, __LINE__, __func__);
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
				check(::poll(fds.data(), fds.size(), no_timeout), __FILE__, __LINE__, __func__);
				std::copy_if(
					fds.begin(), fds.end(),
					std::ostream_iterator<sysx::poll_event>(std::clog, "\n"),
					[] (const sysx::poll_event& rhs) { return !rhs.bad(); }
				);
			}

		}

	}
}
