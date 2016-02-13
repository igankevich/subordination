#include <algorithm>
#include <fstream>

#include <test.hh>

#include <sysx/endpoint.hh>

#include "csv_iterator.hh"

template<class Addr>
struct Test_csv: public test::Test<Test_csv<Addr>> {

	typedef Addr addr_type;

	void xrun() override {
		using namespace discovery;
		std::ifstream in("city.csv");
		typedef csv_iterator<uint32_t,uint32_t> iterator;
		std::for_each(
			iterator(in, ','),
			iterator(),
			[] (const iterator::reference rhs) {
				std::clog << std::get<0>(rhs) << std::endl;
			}
		);
	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test CSV", {
		new Test_csv<sysx::ipv4_addr>
	}};
	return tests.run();
}
