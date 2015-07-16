#include <factory/factory_base.hh>
#include "test.hh"
#include "datum.hh"

using namespace factory;

template<class T>
void test_paired_iterator() {
	std::vector<T> x(20), y(20);
	std::vector<T> z(20), w(20);
	auto beg1 = components::make_paired(x.begin(), y.begin());
	auto end1 = components::make_paired(x.end(), y.end());
	auto beg2 = components::make_paired(z.begin(), w.begin());
	std::copy(beg1, end1, beg2);
	test::compare(x, z);
	test::compare(y, w);
}

int main(int argc, char* argv[]) {
	test_paired_iterator<Datum>();
	return 0;
}
