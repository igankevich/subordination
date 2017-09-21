#ifndef BSCHEDULER_PPL_BASIC_ROUTER_HH
#define BSCHEDULER_PPL_BASIC_ROUTER_HH

#include <bscheduler/ppl/application.hh>
#include <bscheduler/kernel/kernel_header.hh>

namespace bsc {

	template <class T>
	struct basic_router {

		static void
		send_local(T* rhs);

		static void
		send_remote(T*);

		static void
		forward(const kernel_header& hdr, sys::pstream& istr);

		static void
		forward_child(const kernel_header& hdr, sys::pstream& istr);

		static void
		forward_parent(const kernel_header& hdr, sys::pstream& istr);

		static void
		execute(const application& app);

	};

}

#endif // vim:filetype=cpp
