#include <factory/factory.hh>

using namespace factory;

template<class T = uint32_t>
struct Test_endpoint {

	Test_endpoint():
		generator(static_cast<std::default_random_engine::result_type>(current_time_nano())),
		distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max()),
		dice(std::bind(distribution, generator))
	{}

	void test_single() {
		Endpoint addr1(dice(), 0);
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
				msg << "[multiple] Addresses does not match: "
					<< addrs[i] << " /= " << addrs2[i];
				throw std::runtime_error(msg.str());
			}
		}
	}

	void test_variations_ipv4() {
		// basic functionality
		check_read("0.0.0.0:0"             , Endpoint("0.0.0.0"         , 0));
		check_read("0.0.0.0:1234"          , Endpoint("0.0.0.0"         , 1234));
		check_read("0.0.0.0:65535"         , Endpoint("0.0.0.0"         , 65535));
		check_read("10.0.0.1:0"            , Endpoint("10.0.0.1"        , 0));
		check_read("255.0.0.1:0"           , Endpoint("255.0.0.1"       , 0));
		check_read("255.255.255.255:65535" , Endpoint("255.255.255.255" , 65535));
		// out of range ports
		check_read("0.0.0.0:65536"         , Endpoint("0.0.0.0"         , 0));
		check_read("0.0.0.1:65536"         , Endpoint("0.0.0.0"         , 0));
		check_read("10.0.0.1:100000"       , Endpoint("0.0.0.0"         , 0));
		// out of range addrs
		check_read("1000.0.0.1:0"          , Endpoint("0.0.0.0"         , 0));
		// good spaces
		check_read(" 10.0.0.1:100"         , Endpoint("10.0.0.1"        , 100));
		check_read("10.0.0.1:100 "         , Endpoint("10.0.0.1"        , 100));
		check_read(" 10.0.0.1:100 "        , Endpoint("10.0.0.1"        , 100));
		// bad spaces
		check_read("10.0.0.1: 100"         , Endpoint("0.0.0.0"         , 0));
		check_read("10.0.0.1 :100"         , Endpoint("0.0.0.0"         , 0));
		check_read("10.0.0.1 : 100"        , Endpoint("0.0.0.0"         , 0));
		check_read(" 10.0.0.1 : 100 "      , Endpoint("0.0.0.0"         , 0));
		// fancy addrs
		check_read("10:100"                , Endpoint("0.0.0.0"         , 0));
		check_read("10.1:100"              , Endpoint("0.0.0.0"         , 0));
		check_read("10.0.1:100"            , Endpoint("0.0.0.0"         , 0));
		check_read(""                      , Endpoint("0.0.0.0"         , 0));
		check_read("anc:100"               , Endpoint("0.0.0.0"         , 0));
		check_read(":100"                  , Endpoint("0.0.0.0"         , 0));
	}

	void test_variations_ipv6() {
		// basic functionality
		check_read("0:0:0:0:0:0:0:0:0"     , Endpoint("0:0:0:0:0:0:0:0"  , 0));
		check_read("0:0:0:0:0:0:0:0:1234"  , Endpoint("0:0:0:0:0:0:0:0"  , 1234));
		check_read("0:0:0:0:0:0:0:0:65535" , Endpoint("0:0:0:0:0:0:0:0"  , 65535));
		check_read("10:1:0:1:0:0:0:0:0"    , Endpoint("10:1:0:1:0:0:0:0" , 0));
		check_read("255:0:0:1:1:2:3:4:0"   , Endpoint("255:0:0:1:1:2:3:4", 0));
		check_read("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:65535",
			Endpoint("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" , 65535));
	}

	void test_operators() {
		check_bool(Endpoint(), false);
		check_bool(!Endpoint(), true);
		check_bool(Endpoint("127.0.0.1", 100), true);
		check_bool(!Endpoint("127.0.0.1", 100), false);
		check_bool(Endpoint("127.0.0.1", 0), true);
		check_bool(!Endpoint("127.0.0.1", 0), false);
	}

	void test_io() {
		std::vector<Endpoint> addrs(10);
		std::generate(addrs.begin(), addrs.end(), [this] () { return random_addr(); });

		// write
		Foreign_stream os;
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
			msg << "[multiple] Read/write arrays size do not match: "
				<< addrs.size() << " /= " << addrs2.size();
			throw std::runtime_error(msg.str());
		}

		for (size_t i=0; i<addrs.size(); ++i) {
			if (addrs[i] != addrs2[i]) {
				std::stringstream msg;
				msg << "[multiple] Addresses does not match: "
					<< addrs[i] << " /= " << addrs2[i];
				throw std::runtime_error(msg.str());
			}
		}
	}

private:

	void check_bool(Endpoint x, bool y) {
		if ((x && !y) || (!(x) && y)) {
			throw std::runtime_error("Boolean operator failed.");
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
			msg << "Can not read '" << str << "': '"
				<< addr << "' (read) /= '" << expected_result << "' (expected).";
			throw std::runtime_error(msg.str());
		}
	}

	Endpoint random_addr() { return Endpoint(dice(), 0); }

	std::default_random_engine generator;
	std::uniform_int_distribution<T> distribution;
	std::function<T()> dice;
};

struct App {
	int run(int, char**) {
		try {
			Test_endpoint<uint32_t> test;
			test.test_single();
			test.test_multiple();
			test.test_variations_ipv4();
			test.test_variations_ipv6();
			test.test_operators();
			test.test_io();
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
