#include <factory/factory_base.hh>
#include "libfactory.cc"
#include "test.hh"

using namespace factory;
using factory::components::n_random_bytes;
using factory::unix::operator""_ipv4;
using factory::unix::ipv4_addr;
using factory::unix::ipv6_addr;


template<class T = uint32_t, class IP_addr = ipv4_addr>
struct Test_endpoint {

	typedef std::random_device engine_type;

	void test_single() {
		unix::endpoint addr1 = random_addr();
		unix::endpoint addr2;
		std::stringstream s;
		s << addr1;
		s >> addr2;
		if (addr1 != addr2) {
			std::stringstream msg;
			msg << "[single] Addresses does not match: " << addr1 << " /= " << addr2;
			throw std::runtime_error(msg.str());
		}
	}

	void test_multiple() {
		std::vector<unix::endpoint> addrs(10);
		std::generate(addrs.begin(), addrs.end(), [this] () { return random_addr(); });

		// write
		std::stringstream os;
		std::ostream_iterator<unix::endpoint> oit(os, "\n");
		std::copy(addrs.begin(), addrs.end(), oit);

		// read
		std::vector<unix::endpoint> addrs2;
		std::istream_iterator<unix::endpoint> iit(os), eos;
		std::copy(iit, eos, std::back_inserter(addrs2));

		if (addrs.size() != addrs2.size()) {
			std::stringstream msg;
			msg << "[multiple] Read/write arrays size do not match: "
				<< addrs.size() << " /= " << addrs2.size();
			throw std::runtime_error(msg.str());
		}

		for (size_t i=0; i<addrs.size(); ++i) {
			if (addrs[i] != addrs2[i]) {
				std::stringstream msg;
				msg << "[multiple] Addresses do not match: "
					<< addrs[i] << " /= " << addrs2[i];
				throw std::runtime_error(msg.str());
			}
		}
	}

	void test_variations_ipv4() {
		// basic functionality
		check_read("0.0.0.0:0"             , unix::endpoint({0,0,0,0}         , 0));
		check_read("0.0.0.0:1234"          , unix::endpoint({0,0,0,0}         , 1234));
		check_read("0.0.0.0:65535"         , unix::endpoint({0,0,0,0}         , 65535));
		check_read("10.0.0.1:0"            , unix::endpoint({10,0,0,1}        , 0));
		check_read("255.0.0.1:0"           , unix::endpoint({255,0,0,1}       , 0));
		check_read("255.255.255.255:65535" , unix::endpoint({255,255,255,255} , 65535));
		// out of range ports
		check_read("0.0.0.0:65536"         , unix::endpoint({0,0,0,0}         , 0));
		check_read("0.0.0.1:65536"         , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1:100000"       , unix::endpoint({0,0,0,0}         , 0));
		// out of range addrs
		check_read("1000.0.0.1:0"          , unix::endpoint({0,0,0,0}         , 0));
		// good spaces
		check_read(" 10.0.0.1:100"         , unix::endpoint({10,0,0,1}        , 100));
		check_read("10.0.0.1:100 "         , unix::endpoint({10,0,0,1}        , 100));
		check_read(" 10.0.0.1:100 "        , unix::endpoint({10,0,0,1}        , 100));
		// bad spaces
		check_read("10.0.0.1: 100"         , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1 :100"         , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1 : 100"        , unix::endpoint({0,0,0,0}         , 0));
		check_read(" 10.0.0.1 : 100 "      , unix::endpoint({0,0,0,0}         , 0));
		// fancy addrs
		check_read("10:100"                , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.1:100"              , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.0.1:100"            , unix::endpoint({0,0,0,0}         , 0));
		check_read(""                      , unix::endpoint({0,0,0,0}         , 0));
		check_read("anc:100"               , unix::endpoint({0,0,0,0}         , 0));
		check_read(":100"                  , unix::endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.0.1:100"        , unix::endpoint({0,0,0,0}         , 0));
	}

	void test_variations_ipv6() {
		std::clog << __func__ << std::endl;
		// basic functionality
		check_read("[::1]:0"                 , unix::endpoint({0x0,0,0,0,0,0,0,1}  , 0));
		check_read("[1::1]:0"                , unix::endpoint({0x1,0,0,0,0,0,0,1}  , 0));
		check_read("[::]:0"                  , unix::endpoint({0x0,0,0,0,0,0,0,0}  , 0));
		check_read("[2001:1:0::123]:0"       , unix::endpoint({0x2001,1,0,0,0,0,0,0x123}  , 0));
		check_read("[0:0:0:0:0:0:0:0]:0"     , unix::endpoint({0x0,0,0,0,0,0,0,0}  , 0));
		check_read("[0:0:0:0:0:0:0:0]:1234"  , unix::endpoint({0x0,0,0,0,0,0,0,0}  , 1234));
		check_read("[0:0:0:0:0:0:0:0]:65535" , unix::endpoint({0x0,0,0,0,0,0,0,0}  , 65535));
		check_read("[10:1:0:1:0:0:0:0]:0"    , unix::endpoint({0x10,1,0,1,0,0,0,0} , 0));
		check_read("[255:0:0:1:1:2:3:4]:0"   , unix::endpoint({0x255,0,0,1,1,2,3,4}, 0));
		check_read("[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535",
			unix::endpoint({0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff} , 65535));
		// out of range ports
		check_read("[::1]:65536"             , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[::0]:65536"             , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[::0]:100000"            , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		// out of range addrs
		check_read("[1ffff::1]:0"              , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		// good spaces
		check_read(" [10::1]:100"              , unix::endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		check_read("[10::1]:100 "              , unix::endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		check_read(" [10::1]:100 "             , unix::endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		// bad spaces
		check_read("[10::1]: 100"         , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10::1 ]:100"         , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10::1 ]: 100"        , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read(" [10::1 ]: 100 "      , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		// fancy addrs
		check_read("[::1::1]:0"              , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:::]:0"                 , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:]:0"                   , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[]:0"                    , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("]:0"                     , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:0"                     , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10:0:0:0:0:0:0:0:1]:0"  , unix::endpoint({0,0,0,0,0,0,0,0}  , 0));
		// IPv4 mapped addrs
		check_read("[::ffff:127.1.2.3]:0"    , unix::endpoint({0,0,0,0xffff,0x127,1,2,3}, 0));
	}

	void test_operators() {
		// operator bool
		check_bool(unix::endpoint(), false);
		check_bool(!unix::endpoint(), true);
		// operator bool (IPv4)
		check_bool(unix::endpoint("0.0.0.0", 0), false);
		check_bool(!unix::endpoint("0.0.0.0", 0), true);
		check_bool(unix::endpoint("127.0.0.1", 100), true);
		check_bool(!unix::endpoint("127.0.0.1", 100), false);
		check_bool(unix::endpoint("127.0.0.1", 0), true);
		check_bool(!unix::endpoint("127.0.0.1", 0), false);
		// operator bool (IPv6)
		check_bool(unix::endpoint("0:0:0:0:0:0:0:0", 0), false);
		check_bool(!unix::endpoint("0:0:0:0:0:0:0:0", 0), true);
		check_bool(unix::endpoint("::", 0), false);
		check_bool(!unix::endpoint("::", 0), true);
		check_bool(unix::endpoint("::1", 0), true);
		check_bool(!unix::endpoint("::1", 0), false);
		// comparison operators
		check_bool(unix::endpoint("::", 0) == unix::endpoint(), true);
		check_bool(unix::endpoint("0:0:0:0:0:0:0:0", 0) == unix::endpoint(), true);
		check_bool(unix::endpoint("0.0.0.0", 0) == unix::endpoint(), true);
		check_bool(unix::endpoint("::", 0) != unix::endpoint("0.0.0.0", 0), true);
		// ordering
		check_bool(unix::endpoint("10.0.0.1", 0) < unix::endpoint("10.0.0.2", 0), true);
		check_bool(unix::endpoint("10.0.0.2", 0) < unix::endpoint("10.0.0.1", 0), false);
		check_bool(unix::endpoint("10::1", 0) < unix::endpoint("10::2", 0), true);
		check_bool(unix::endpoint("10::2", 0) < unix::endpoint("10::1", 0), false);
		check_bool(unix::endpoint("10.0.0.1", 0) < unix::endpoint("10::1", 0), true);
		// copying
		test::equal(unix::endpoint(unix::endpoint("10.0.0.1", 1234), 100), unix::endpoint("10.0.0.1", 100));
	}

	void test_io() {
		std::vector<unix::endpoint> addrs(10);
		std::generate(addrs.begin(), addrs.end(), [this] () { return random_addr(); });

		// write
		std::stringbuf buf;
		packstream os(&buf);
		std::for_each(addrs.begin(), addrs.end(), [&os] (const unix::endpoint& rhs) {
			os << rhs;
		});

		// read
		std::vector<unix::endpoint> addrs2(addrs.size());
		std::for_each(addrs2.begin(), addrs2.end(), [&os] (unix::endpoint& rhs) {
			os >> rhs;
		});

		if (addrs.size() != addrs2.size()) {
			std::stringstream msg;
			msg << "[io] Read/write arrays size do not match: "
				<< addrs.size() << " /= " << addrs2.size();
			throw std::runtime_error(msg.str());
		}

		for (size_t i=0; i<addrs.size(); ++i) {
			if (addrs[i] != addrs2[i]) {
				std::stringstream msg;
				msg << "[io] Addresses does not match: "
					<< addrs[i] << " /= " << addrs2[i];
				throw std::runtime_error(msg.str());
			}
		}
	}

	void test_literals() {
		constexpr ipv4_addr any4;
		constexpr ipv6_addr any6;
		constexpr unix::endpoint any;
		std::cout << "192.168.33.77"_ipv4 << std::endl;
		constexpr unix::endpoint endp("192.168.33.77"_ipv4,0);
		std::cout << endp << std::endl;
		constexpr unix::endpoint endpX(unix::endpoint("10.0.0.1"_ipv4, 1234), 100);
		constexpr unix::endpoint endpY("10.0.0.1"_ipv4, 100);
		test::equal(endpX, endpY);
		constexpr unix::endpoint endpU(unix::endpoint(ipv6_addr(), 1234), 100);
		constexpr unix::endpoint endpV(ipv6_addr(), 100);
		test::equal(endpU, endpV);
	}

private:

	void check_bool(unix::endpoint x, bool y) {
		if ((x && !y) || (!(x) && y)) {
			std::stringstream msg;
			msg << "Boolean operator failed. unix::endpoint=" << x;
			throw std::runtime_error(msg.str());
		}
	}

	void check_bool(bool x, bool y) {
		if (x != y) {
			throw std::runtime_error("Boolean operator failed.");
		}
	}

	void check_read(const char* str, unix::endpoint expected_result) {
		unix::endpoint addr;
		std::stringstream s;
		s << str;
		s >> addr;
		if (addr != expected_result) {
			std::stringstream msg;
			msg << "Read failed for '" << str << "': '"
				<< addr << "' (read) /= '" << expected_result << "' (expected).";
			throw std::runtime_error(msg.str());
		}
	}

	unix::endpoint random_addr() { return unix::endpoint(IP_addr(n_random_bytes<T>(generator)), 0); }

	engine_type generator;
};

struct App {
	void test_ipv4() {
		Test_endpoint<uint32_t, ipv4_addr> test;
		test.test_single();
		test.test_multiple();
		test.test_variations_ipv4();
		test.test_operators();
		test.test_io();
		test.test_literals();
	}
	void test_ipv6() {
		Test_endpoint<std::uint128_t, ipv6_addr> test;
		test.test_single();
		test.test_multiple();
		test.test_variations_ipv6();
		test.test_operators();
		test.test_io();
	}
	int run(int, char**) {
		try {
			test_ipv4();
			test_ipv6();
//		std::clog << unix::endpoint({192, 168, 0, 1}, 0) << std::endl;
		std::clog << unix::endpoint({10, 0, 0, 0, 0, 0, 0, 1}, 0) << std::endl;
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
