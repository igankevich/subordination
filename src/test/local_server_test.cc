#include <gtest/gtest.h>
#include <bscheduler/ppl/local_server.hh>
#include <test/make_types.hh>

template<class Pair>
struct LocalServerTest: public ::testing::Test {};

TYPED_TEST_CASE(
	LocalServerTest,
	MAKE_TYPES(
		sys::ipv4_address,
		sys::ipv6_address
	)
);

TYPED_TEST(LocalServerTest, DetermineIdRange) {
	typedef TypeParam addr_type;
	typedef sys::interface_address<addr_type> ifaddr_type;
	typedef sys::ipaddr_traits<addr_type> traits_type;
	typedef typename addr_type::rep_type id_type;
	ifaddr_type interface_address(traits_type::localhost(), traits_type::loopback_mask());
	id_type id0 = 0;
	id_type id1 = 0;
	bsc::determine_id_range(interface_address, id0, id1);
}

template <class Addr, class Id>
void
test_id_count(const Id& expected, const sys::interface_address<Addr>& interface_address) {
	typedef Id id_type;
	id_type id0 = 0;
	id_type id1 = 0;
	bsc::determine_id_range(interface_address, id0, id1);
	EXPECT_EQ(expected, id1-id0) << "id0=" << id0 << ",id1=" << id1;
}

TEST(LocalServer, DetermineIdRangeIpV4) {
	typedef sys::ipv4_address addr_type;
	typedef sys::interface_address<addr_type> ifaddr_type;
	typedef typename addr_type::rep_type id_type;
	test_id_count(id_type(255), ifaddr_type{{127,0,0,1}, 8});
	test_id_count(id_type(256), ifaddr_type{{127,0,0,2}, 8});
	test_id_count(id_type(1), ifaddr_type{{127,0,0,1}, 0});
	test_id_count(id_type(1), ifaddr_type{{127,0,0,2}, 0});
	test_id_count(id_type(1), ifaddr_type{{127,0,0,1}, 32});
	test_id_count(id_type(1), ifaddr_type{{127,0,0,2}, 32});
}

TEST(LocalServer, IntersectIpV4) {
	typedef sys::ipv4_address addr_type;
	typedef sys::interface_address<addr_type> ifaddr_type;
	typedef typename addr_type::rep_type id_type;
	ifaddr_type interface_address{{127,0,0,1}, 24};
	const id_type cnt = interface_address.count();
	id_type id0 = 1;
	id_type id1 = 1;
	id_type id2 = 0;
	id_type id3 = 0;
	for (id_type i=0; i<cnt; ++i) {
		EXPECT_LE(i+1, std::numeric_limits<unsigned char>::max());
		unsigned char uc = i + 1;
		ifaddr_type new_ifaddr{{127,0,0,uc}, 24};
		bsc::determine_id_range(new_ifaddr, id2, id3);
		EXPECT_LE(id0, id1);
		EXPECT_EQ(id1, id2);
		EXPECT_LE(id2, id3);
		id0 = id2;
		id1 = id3;
	}
}
