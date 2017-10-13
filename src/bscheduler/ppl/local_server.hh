#ifndef BSCHEDULER_PPL_LOCAL_SERVER_HH
#define BSCHEDULER_PPL_LOCAL_SERVER_HH

#include <limits>

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

#include <bscheduler/ppl/basic_handler.hh>

namespace bsc {

	template <class Addr>
	inline void
	determine_id_range(
		const sys::ifaddr<Addr>& ifaddr,
		typename Addr::rep_type& first,
		typename Addr::rep_type& last
	) noexcept {
		typedef typename Addr::rep_type id_type;
		using std::max;
		using std::min;
		const id_type n = ifaddr.count();
		if (n == 0) {
			first = 0;
			last = 0;
			return;
		}
		const id_type min_id = std::numeric_limits<id_type>::min();
		const id_type max_id = std::numeric_limits<id_type>::max();
		const id_type step = (max_id-min_id) / n;
		const id_type pos = ifaddr.position();
		first = max(id_type(1), min_id + step*(pos-1));
		last = min(max_id, min_id + step*(pos));
	}

}

#endif // vim:filetype=cpp
