#include <algorithm>
#include <fstream>
#include <iterator>

#include <test.hh>

#include <sysx/endpoint.hh>

#include "csv_iterator.hh"

template<class Addr>
struct Test_csv: public test::Test<Test_csv<Addr>> {

	typedef Addr addr_type;

	void xrun() override {
		using namespace discovery;
		std::ifstream in("city.csv");
		typedef csv_tuple<',',std::string> mytuple;
		std::copy(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			std::ostream_iterator<mytuple>(std::clog, "\n")
		);
	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test CSV", {
		new Test_csv<sysx::ipv4_addr>
	}};
	return tests.run();
}
