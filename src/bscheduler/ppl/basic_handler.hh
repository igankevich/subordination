#ifndef BSCHEDULER_PPL_BASIC_HANDLER_HH
#define BSCHEDULER_PPL_BASIC_HANDLER_HH

#include <iosfwd>

#include <unistdx/io/epoll_event>

#include <bscheduler/ppl/pipeline_base.hh>

namespace bsc {

	class basic_handler: public pipeline_base {

	public:

		virtual void
		handle(const sys::epoll_event& ev) = 0;

		virtual void
		write(std::ostream& out) const = 0;

		inline friend std::ostream&
		operator<<(std::ostream& out, const basic_handler& rhs) {
			rhs.write(out);
			return out;
		}

	};

}

#endif // vim:filetype=cpp
