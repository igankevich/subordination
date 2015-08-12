#include <factory/factory_base.hh>
#include "libfactory.cc"
#include "test.hh"

using namespace factory;


template<class T = uint32_t, class IP_addr = IPv4_addr>
struct Test_endpoint {

	typedef factory::components::n_random_bytes_engine<
		std::random_device, T> engine_type;

	void test_single() {
		Endpoint addr1 = random_addr();
		Endpoint addr2;
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
		std::vector<Endpoint> addrs(10);
		std::generate(addrs.begin(), addrs.end(), [this] () { return random_addr(); });

		// write
		std::stringstream os;
		std::ostream_iterator<Endpoint> oit(os, "\n");
		std::copy(addrs.begin(), addrs.end(), oit);

		// read
		std::vector<Endpoint> addrs2;
		std::istream_iterator<Endpoint> iit(os), eos;
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
		check_read("0.0.0.0:0"             , Endpoint({0,0,0,0}         , 0));
		check_read("0.0.0.0:1234"          , Endpoint({0,0,0,0}         , 1234));
		check_read("0.0.0.0:65535"         , Endpoint({0,0,0,0}         , 65535));
		check_read("10.0.0.1:0"            , Endpoint({10,0,0,1}        , 0));
		check_read("255.0.0.1:0"           , Endpoint({255,0,0,1}       , 0));
		check_read("255.255.255.255:65535" , Endpoint({255,255,255,255} , 65535));
		// out of range ports
		check_read("0.0.0.0:65536"         , Endpoint({0,0,0,0}         , 0));
		check_read("0.0.0.1:65536"         , Endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1:100000"       , Endpoint({0,0,0,0}         , 0));
		// out of range addrs
		check_read("1000.0.0.1:0"          , Endpoint({0,0,0,0}         , 0));
		// good spaces
		check_read(" 10.0.0.1:100"         , Endpoint({10,0,0,1}        , 100));
		check_read("10.0.0.1:100 "         , Endpoint({10,0,0,1}        , 100));
		check_read(" 10.0.0.1:100 "        , Endpoint({10,0,0,1}        , 100));
		// bad spaces
		check_read("10.0.0.1: 100"         , Endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1 :100"         , Endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.1 : 100"        , Endpoint({0,0,0,0}         , 0));
		check_read(" 10.0.0.1 : 100 "      , Endpoint({0,0,0,0}         , 0));
		// fancy addrs
		check_read("10:100"                , Endpoint({0,0,0,0}         , 0));
		check_read("10.1:100"              , Endpoint({0,0,0,0}         , 0));
		check_read("10.0.1:100"            , Endpoint({0,0,0,0}         , 0));
		check_read(""                      , Endpoint({0,0,0,0}         , 0));
		check_read("anc:100"               , Endpoint({0,0,0,0}         , 0));
		check_read(":100"                  , Endpoint({0,0,0,0}         , 0));
		check_read("10.0.0.0.1:100"        , Endpoint({0,0,0,0}         , 0));
	}

	void test_variations_ipv6() {
		std::clog << __func__ << std::endl;
		// basic functionality
		check_read("[::1]:0"                 , Endpoint({0x0,0,0,0,0,0,0,1}  , 0));
		check_read("[1::1]:0"                , Endpoint({0x1,0,0,0,0,0,0,1}  , 0));
		check_read("[::]:0"                  , Endpoint({0x0,0,0,0,0,0,0,0}  , 0));
		check_read("[2001:1:0::123]:0"       , Endpoint({0x2001,1,0,0,0,0,0,0x123}  , 0));
		check_read("[0:0:0:0:0:0:0:0]:0"     , Endpoint({0x0,0,0,0,0,0,0,0}  , 0));
		check_read("[0:0:0:0:0:0:0:0]:1234"  , Endpoint({0x0,0,0,0,0,0,0,0}  , 1234));
		check_read("[0:0:0:0:0:0:0:0]:65535" , Endpoint({0x0,0,0,0,0,0,0,0}  , 65535));
		check_read("[10:1:0:1:0:0:0:0]:0"    , Endpoint({0x10,1,0,1,0,0,0,0} , 0));
		check_read("[255:0:0:1:1:2:3:4]:0"   , Endpoint({0x255,0,0,1,1,2,3,4}, 0));
		check_read("[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535",
			Endpoint({0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff} , 65535));
		// out of range ports
		check_read("[::1]:65536"             , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[::0]:65536"             , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[::0]:100000"            , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		// out of range addrs
		check_read("[1ffff::1]:0"              , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		// good spaces
		check_read(" [10::1]:100"              , Endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		check_read("[10::1]:100 "              , Endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		check_read(" [10::1]:100 "             , Endpoint({0x10,0,0,0,0,0,0,1}        , 100));
		// bad spaces
		check_read("[10::1]: 100"         , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10::1 ]:100"         , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10::1 ]: 100"        , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read(" [10::1 ]: 100 "      , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		// fancy addrs
		check_read("[::1::1]:0"              , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:::]:0"                 , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:]:0"                   , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[]:0"                    , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("]:0"                     , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[:0"                     , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		check_read("[10:0:0:0:0:0:0:0:1]:0"  , Endpoint({0,0,0,0,0,0,0,0}  , 0));
		// IPv4 mapped addrs
		check_read("[::ffff:127.1.2.3]:0"    , Endpoint({0,0,0,0xffff,0x127,1,2,3}, 0));
	}

	void test_operators() {
		// operator bool
		check_bool(Endpoint(), false);
		check_bool(!Endpoint(), true);
		// operator bool (IPv4)
		check_bool(Endpoint("0.0.0.0", 0), false);
		check_bool(!Endpoint("0.0.0.0", 0), true);
		check_bool(Endpoint("127.0.0.1", 100), true);
		check_bool(!Endpoint("127.0.0.1", 100), false);
		check_bool(Endpoint("127.0.0.1", 0), true);
		check_bool(!Endpoint("127.0.0.1", 0), false);
		// operator bool (IPv6)
		check_bool(Endpoint("0:0:0:0:0:0:0:0", 0), false);
		check_bool(!Endpoint("0:0:0:0:0:0:0:0", 0), true);
		check_bool(Endpoint("::", 0), false);
		check_bool(!Endpoint("::", 0), true);
		check_bool(Endpoint("::1", 0), true);
		check_bool(!Endpoint("::1", 0), false);
		// comparison operators
		check_bool(Endpoint("::", 0) == Endpoint(), true);
		check_bool(Endpoint("0:0:0:0:0:0:0:0", 0) == Endpoint(), true);
		check_bool(Endpoint("0.0.0.0", 0) == Endpoint(), true);
		check_bool(Endpoint("::", 0) != Endpoint("0.0.0.0", 0), true);
		// ordering
		check_bool(Endpoint("10.0.0.1", 0) < Endpoint("10.0.0.2", 0), true);
		check_bool(Endpoint("10.0.0.2", 0) < Endpoint("10.0.0.1", 0), false);
		check_bool(Endpoint("10::1", 0) < Endpoint("10::2", 0), true);
		check_bool(Endpoint("10::2", 0) < Endpoint("10::1", 0), false);
		check_bool(Endpoint("10.0.0.1", 0) < Endpoint("10::1", 0), true);
		// copying
		test::equal(Endpoint(Endpoint("10.0.0.1", 1234), 100), Endpoint("10.0.0.1", 100));
	}

	void test_io() {
		std::vector<Endpoint> addrs(10);
		std::generate(addrs.begin(), addrs.end(), [this] () { return random_addr(); });

		// write
		std::stringbuf buf;
		packstream os(&buf);
		std::for_each(addrs.begin(), addrs.end(), [&os] (const Endpoint& rhs) {
			os << rhs;
		});

		// read
		std::vector<Endpoint> addrs2(addrs.size());
		std::for_each(addrs2.begin(), addrs2.end(), [&os] (Endpoint& rhs) {
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
		constexpr IPv4_addr any4;
		constexpr IPv6_addr any6;
		constexpr Endpoint any;
		std::cout << "192.168.33.77"_ipv4 << std::endl;
		constexpr Endpoint endp("192.168.33.77"_ipv4,0);
		std::cout << endp << std::endl;
		constexpr Endpoint endpX(Endpoint("10.0.0.1"_ipv4, 1234), 100);
		constexpr Endpoint endpY("10.0.0.1"_ipv4, 100);
		test::equal(endpX, endpY);
		constexpr Endpoint endpU(Endpoint(IPv6_addr(), 1234), 100);
		constexpr Endpoint endpV(IPv6_addr(), 100);
		test::equal(endpU, endpV);
	}

private:

	void check_bool(Endpoint x, bool y) {
		if ((x && !y) || (!(x) && y)) {
			std::stringstream msg;
			msg << "Boolean operator failed. Endpoint=" << x;
			throw std::runtime_error(msg.str());
		}
	}

	void check_bool(bool x, bool y) {
		if (x != y) {
			throw std::runtime_error("Boolean operator failed.");
		}
	}

	void check_read(const char* str, Endpoint expected_result) {
		Endpoint addr;
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

	Endpoint random_addr() { return Endpoint(IP_addr(generator()), 0); }

	engine_type generator;
};

struct App {
	void test_ipv4() {
		Test_endpoint<uint32_t, IPv4_addr> test;
		test.test_single();
		test.test_multiple();
		test.test_variations_ipv4();
		test.test_operators();
		test.test_io();
		test.test_literals();
	}
	void test_ipv6() {
		Test_endpoint<std::uint128_t, IPv6_addr> test;
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
//		std::clog << Endpoint({192, 168, 0, 1}, 0) << std::endl;
		std::clog << Endpoint({10, 0, 0, 0, 0, 0, 0, 1}, 0) << std::endl;
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
