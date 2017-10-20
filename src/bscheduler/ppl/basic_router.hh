#ifndef BSCHEDULER_PPL_BASIC_ROUTER_HH
#define BSCHEDULER_PPL_BASIC_ROUTER_HH

#include <bscheduler/ppl/application.hh>
#include <bscheduler/kernel/kernel_header.hh>

namespace bsc {

	template <class T>
	struct basic_router {

		typedef T kernel_type;

		static void
		send_local(T* rhs);

		static void
		send_remote(T*);

		static void
		forward(foreign_kernel* hdr);

		static void
		forward_child(foreign_kernel* hdr);

		static void
		forward_parent(foreign_kernel* hdr);

		static void
		execute(const application& app);

	};

}

#endif // vim:filetype=cpp
