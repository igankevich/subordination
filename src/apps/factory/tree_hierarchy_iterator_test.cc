#include "tree_hierarchy_iterator.hh"
#include <gtest/gtest.h>
#include <unistdx/base/log_message>

template <class Addr, class Iterator>
std::vector<Addr>
generate_addresses(Iterator it, int n) {
	std::vector<Addr> result;
	for (int i=0; i<n; ++i) {
		result.emplace_back(*it);
		++it;
	}
	return result;
}

TEST(TreeHierarchyIterator, Fanout2) {
	// no. of layers = 3
	// fanout = 2
	typedef sys::ipv4_addr addr_type;
	typedef factory::tree_hierarchy_iterator<addr_type> iterator;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef std::vector<addr_type> container_type;
	EXPECT_EQ(
		container_type(),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,1}, 8}, 2), 0)
	);
	EXPECT_EQ(
		container_type({{127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,2}, 8}, 2), 1)
	);
	sys::log_message("tst", "3");
	EXPECT_EQ(
		container_type({{127,0,0,1}, {127,0,0,2}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,3}, 8}, 2), 2)
	);
	sys::log_message("tst", "4");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}, {127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,4}, 8}, 2), 3)
	);
	sys::log_message("tst", "5");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}, {127,0,0,1}, {127,0,0,4}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,5}, 8}, 2), 4)
	);
	sys::log_message("tst", "6");
	EXPECT_EQ(
		container_type({
			{127,0,0,3},
			{127,0,0,2},
			{127,0,0,1},
			{127,0,0,4},
			{127,0,0,5}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,6}, 8}, 2), 5)
	);
	sys::log_message("tst", "7");
	EXPECT_EQ(
		container_type({
			{127,0,0,3},
			{127,0,0,2},
			{127,0,0,1},
			{127,0,0,4},
			{127,0,0,5},
			{127,0,0,6}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,7}, 8}, 2), 6)
	);
}
