#include "tree_hierarchy_iterator.hh"
#include <gtest/gtest.h>
#include <unistdx/base/log_message>

template <class Addr, class Iterator>
std::vector<Addr>
generate_addresses(Iterator it) {
	std::vector<Addr> result;
	Iterator last;
	while (it != last)  {
		result.emplace_back(*it);
		++it;
	}
	return result;
}

TEST(TreeHierarchyIterator, Fanout2) {
	// no. of layers = 3
	// fanout = 2
	const int fanout = 2;
	typedef sys::ipv4_addr addr_type;
	typedef bsc::tree_hierarchy_iterator<addr_type> iterator;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef std::vector<addr_type> container_type;
	sys::log_message("tst", "1");
	EXPECT_EQ(
		container_type(),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,1}, 8}, fanout))
	);
	sys::log_message("tst", "2");
	EXPECT_EQ(
		container_type({{127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,2}, 8}, fanout))
	);
	sys::log_message("tst", "3");
	EXPECT_EQ(
		container_type({{127,0,0,1}, {127,0,0,2}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,3}, 8}, fanout))
	);
	sys::log_message("tst", "4");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}, {127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,4}, 8}, fanout))
	);
	sys::log_message("tst", "5");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}, {127,0,0,1}, {127,0,0,4}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,5}, 8}, fanout))
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
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,6}, 8}, fanout))
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
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,7}, 8}, fanout))
	);
}

TEST(TreeHierarchyIterator, Fanout3) {
	// no. of layers = 3
	// fanout = 3
	const int fanout = 3;
	typedef sys::ipv4_addr addr_type;
	typedef bsc::tree_hierarchy_iterator<addr_type> iterator;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef std::vector<addr_type> container_type;
	sys::log_message("tst", "1");
	EXPECT_EQ(
		container_type(),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,1}, 8}, fanout))
	);
	sys::log_message("tst", "2");
	EXPECT_EQ(
		container_type({{127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,2}, 8}, fanout))
	);
	sys::log_message("tst", "3");
	EXPECT_EQ(
		container_type({{127,0,0,1}, {127,0,0,2}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,3}, 8}, fanout))
	);
	sys::log_message("tst", "4");
	EXPECT_EQ(
		container_type({{127,0,0,1}, {127,0,0,2}, {127,0,0,3}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,4}, 8}, fanout))
	);
	sys::log_message("tst", "5");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}, {127,0,0,4}, {127,0,0,1}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,5}, 8}, fanout))
	);
	sys::log_message("tst", "6");
	EXPECT_EQ(
		container_type({
			{127,0,0,2},
			{127,0,0,3},
			{127,0,0,4},
			{127,0,0,1},
			{127,0,0,5}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,6}, 8}, fanout))
	);
	sys::log_message("tst", "7");
	EXPECT_EQ(
		container_type({
			{127,0,0,2},
			{127,0,0,3},
			{127,0,0,4},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,7}, 8}, fanout))
	);
	sys::log_message("tst", "8");
	EXPECT_EQ(
		container_type({
			{127,0,0,3},
			{127,0,0,2},
			{127,0,0,4},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6},
			{127,0,0,7}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,8}, 8}, fanout))
	);
	sys::log_message("tst", "9");
	EXPECT_EQ(
		container_type({
			{127,0,0,3},
			{127,0,0,2},
			{127,0,0,4},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6},
			{127,0,0,7},
			{127,0,0,8}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,9}, 8}, fanout))
	);
	sys::log_message("tst", "10");
	EXPECT_EQ(
		container_type({
			{127,0,0,3},
			{127,0,0,2},
			{127,0,0,4},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6},
			{127,0,0,7},
			{127,0,0,8},
			{127,0,0,9}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,10}, 8}, fanout))
	);
	sys::log_message("tst", "11");
	EXPECT_EQ(
		container_type({
			{127,0,0,4},
			{127,0,0,2},
			{127,0,0,3},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6},
			{127,0,0,7},
			{127,0,0,8},
			{127,0,0,9},
			{127,0,0,10}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,11}, 8}, fanout))
	);
	sys::log_message("tst", "12");
	EXPECT_EQ(
		container_type({
			{127,0,0,4},
			{127,0,0,2},
			{127,0,0,3},
			{127,0,0,1},
			{127,0,0,5},
			{127,0,0,6},
			{127,0,0,7},
			{127,0,0,8},
			{127,0,0,9},
			{127,0,0,10},
			{127,0,0,11}
		}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,12}, 8}, fanout))
	);
}

TEST(TreeHierarchyIterator, NonZeroOffset) {
    // no. of layers = 3
    // fanout = 2
    // offset = 1
	const int fanout = 3;
	const int offset = 1;
	typedef sys::ipv4_addr addr_type;
	typedef bsc::tree_hierarchy_iterator<addr_type> iterator;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef std::vector<addr_type> container_type;
	sys::log_message("tst", "1");
	EXPECT_EQ(
		container_type(),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,1}, 8}, fanout, offset))
	);
	sys::log_message("tst", "2");
	EXPECT_EQ(
		container_type(),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,2}, 8}, fanout, offset))
	);
	sys::log_message("tst", "3");
	EXPECT_EQ(
		container_type({{127,0,0,2}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,3}, 8}, fanout, offset))
	);
	sys::log_message("tst", "4");
	EXPECT_EQ(
		container_type({{127,0,0,2}, {127,0,0,3}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,4}, 8}, fanout, offset))
	);
}

TEST(TreeHierarchyIterator, LargeFanOut) {
    // no. of layers = 3
    // fanout = 2
    // offset = 1
	const int fanout = 20000;
	typedef sys::ipv4_addr addr_type;
	typedef bsc::tree_hierarchy_iterator<addr_type> iterator;
	typedef sys::ifaddr<addr_type> ifaddr_type;
	typedef std::vector<addr_type> container_type;
	EXPECT_EQ(
		container_type({{127,0,0,1}, {127,0,0,2}, {127,0,0,3}, {127,0,0,4}}),
		generate_addresses<addr_type>(iterator(ifaddr_type{{127,0,0,5}, 8}, fanout))
	);
}
