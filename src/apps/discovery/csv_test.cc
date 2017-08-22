#include <algorithm>
#include <iterator>

#include <test.hh>

#include "csv_tuple.hh"

struct Test_synthetic: public test::Test<Test_synthetic> {

	void xrun() override {
		syntethic_test(100);
	}

private:

	void
	syntethic_test(size_t n) {
		using discovery::csv_tuple;
		typedef std::pair<uint32_t,uint32_t> value_type;
		typedef csv_tuple<',',uint32_t,uint32_t> mytuple;
		std::vector<value_type> dataset(n);
		test::randomise(dataset.begin(), dataset.end(), _rng);
		std::stringstream stream;
		std::for_each(
			dataset.begin(),
			dataset.end(),
			[&stream] (const value_type& rhs) {
				stream << rhs.first << ',' << rhs.second << '\n';
			}
		);
		std::vector<value_type> result;
		std::transform(
			std::istream_iterator<mytuple>(stream),
			std::istream_iterator<mytuple>(),
			std::back_inserter(result),
			[] (const mytuple& rhs) {
				return std::make_pair(std::get<0>(rhs), std::get<1>(rhs));
			}
		);
		test::compare_bytes(result, dataset, "result does not match dataset");
	}

private:

	std::random_device _rng;

};

struct Test_missing_values: public test::Test<Test_missing_values> {

	typedef discovery::csv_tuple<',',uint32_t,uint32_t,uint32_t> mytuple;
	typedef std::tuple<uint32_t,uint32_t,uint32_t> value_type;

	void xrun() override {
		std::stringstream stream;
		stream << "1,,2\n,,4\n1,,";
		std::vector<value_type> result;
		std::copy(
			std::istream_iterator<mytuple>(stream),
			std::istream_iterator<mytuple>(),
			std::back_inserter(result)
		);
		test::equal(result.size(), 3u, "bad size");
		// first line
		test::equal(std::get<0>(result[0]), 1u, "bad first line");
		test::equal(std::get<1>(result[0]), 0u, "bad first line");
		test::equal(std::get<2>(result[0]), 2u, "bad first line");
		// second line
		test::equal(std::get<0>(result[1]), 0u, "bad second line");
		test::equal(std::get<1>(result[1]), 0u, "bad second line");
		test::equal(std::get<2>(result[1]), 4u, "bad second line");
		// third line
		test::equal(std::get<0>(result[2]), 1u, "bad third line");
		test::equal(std::get<1>(result[2]), 0u, "bad third line");
		test::equal(std::get<2>(result[2]), 0u, "bad third line");
	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test CSV", {
		new Test_synthetic,
		new Test_missing_values
	}};
	return tests.run();
}
