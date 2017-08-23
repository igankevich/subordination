#include <vector>
#include <iterator>
#include <map>
#include <algorithm>

#include <unistdx/base/n_random_bytes>
#include <unistdx/base/make_object>
#include <unistdx/net/ifaddrs>
#include <gtest/gtest.h>

#include "distance.hh"


namespace std {

	template<class X, class Y>
	std::ostream&
	operator<<(std::ostream& out, const std::pair<X,Y>& rhs) {
		return out << sys::make_object("rank", rhs.first, "addr", rhs.second);
	}

	template <class Distance, class Addr>
	std::ostream&
	operator<<(
		std::ostream& out,
		const std::multimap<Distance,Addr> rhs
	) {
		typedef std::pair<const Distance,Addr> pair_type;
		std::copy(
			rhs.begin(), rhs.end(),
			std::ostream_iterator<pair_type>(out, "\n")
		);
		return out;
	}

}

typedef sys::ifaddr<sys::ipv4_addr> ipv4_ifaddr;
typedef discovery::Distance_in_tree<sys::ipv4_addr> distance_type;
using sys::ipv4_addr;

void
test_ranking_algorithm(
	const std::initializer_list<ipv4_ifaddr>& ifaddrs,
	const ipv4_addr::rep_type fanout,
	ipv4_addr expected
) {
	std::multimap<distance_type,ipv4_addr> ranked_hosts;
	std::for_each(
		ifaddrs.begin(), ifaddrs.end(),
		[&ranked_hosts,fanout] (const ipv4_ifaddr& rhs) {
			std::transform(
				rhs.begin(), rhs.middle(),
				std::inserter(ranked_hosts, ranked_hosts.end()),
				[&rhs,fanout] (const ipv4_addr& to) {
					const distance_type dist = distance_type(
						rhs.address(),
						to,
						rhs.netmask(),
						fanout
					);
					return std::make_pair(dist, to);
				}
			);
		}
	);
	ipv4_addr result = ranked_hosts.empty() ? ipv4_addr{} : ranked_hosts.begin()->second;
	EXPECT_EQ(expected, result)
		<< "ranking does not work (fanout=" << fanout << ")\n"
		"Expected " << expected << " be the first in the following list:"
		<< ranked_hosts;
}

void
debug_addrs() {
	std::clog << distance_type(2, ipv4_addr{127,0,0,1}, ipv4_addr{255,0,0,0}) << std::endl;
	std::clog << distance_type(2, ipv4_addr{127,0,0,2}, ipv4_addr{255,0,0,0}) << std::endl;
	std::clog << distance_type(2, ipv4_addr{127,0,0,3}, ipv4_addr{255,0,0,0}) << std::endl;
	std::clog << distance_type(2, ipv4_addr{127,0,0,4}, ipv4_addr{255,0,0,0}) << std::endl;
	std::clog
		<< std::setw(20) << "address"
		<< std::setw(20) << "position"
		<< std::setw(20) << "position in tree"
		<< std::endl;
	std::clog << std::right;
	for (unsigned char i=1; i<23; ++i) {
		const ipv4_addr addr{127,0,0,i};
		std::clog
			<< std::setw(20) << addr
			<< std::setw(20) << addr.position(ipv4_addr{255,0,0,0})
			<< std::setw(20) << distance_type(2, addr, ipv4_addr{255,0,0,0})
			<< std::endl;
	}
}

ipv4_addr
genaddr(ipv4_addr::oct_type last_octet) {
	return {127,0,0,last_octet};
}

ipv4_ifaddr
genifaddr(ipv4_addr::oct_type last_octet) {
	return {{127,0,0,last_octet}, {255,0,0,0}};
}

TEST(DistanceInTree, Static) {
	EXPECT_EQ(
		distance_type(
			sys::ipv4_addr(127,0,0,2),
			sys::ipv4_addr(127,0,0,1),
			sys::ipv4_addr(255,0,0,0),
			2
		),
		distance_type(1, 0)
	);
}

TEST(DistanceInTree, Ranking) {
	const ipv4_addr netmask{255,0,0,0};
	test_ranking_algorithm({genifaddr(1)}, 2, {});
	test_ranking_algorithm({genifaddr(2)}, 2, {127,0,0,1});
	test_ranking_algorithm({genifaddr(3)}, 2, {127,0,0,1});
	test_ranking_algorithm({genifaddr(4)}, 2, {127,0,0,2});
	test_ranking_algorithm({genifaddr(5)}, 2, {127,0,0,2});
	test_ranking_algorithm({genifaddr(6)}, 2, {127,0,0,3});
	test_ranking_algorithm({genifaddr(7)}, 2, {127,0,0,3});
	test_ranking_algorithm({genifaddr(8)}, 2, {127,0,0,4});
	test_ranking_algorithm({genifaddr(4)}, 4, {127,0,0,1});
	test_ranking_algorithm({genifaddr(5)}, 4, {127,0,0,1});
	test_ranking_algorithm({genifaddr(6)}, 4, {127,0,0,2});
	test_ranking_algorithm({genifaddr(7)}, 4, {127,0,0,2});
}

TEST(EnumerateIfaddrs, All) {
	sys::enumerate_ifaddrs<sys::ipv4_addr>(
		std::ostream_iterator<ipv4_ifaddr>(std::clog, "\n")
	);
}
