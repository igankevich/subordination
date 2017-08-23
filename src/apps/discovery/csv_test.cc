#include <algorithm>
#include <iterator>

#include <test.hh>

#include "csv_tuple.hh"

std::random_device rng;

TEST(CsvTuple, Synthetic) {
	size_t n = 100;
	using discovery::csv_tuple;
	typedef std::pair<uint32_t,uint32_t> value_type;
	typedef csv_tuple<',',uint32_t,uint32_t> mytuple;
	std::vector<value_type> dataset(n);
	test::randomise(dataset.begin(), dataset.end(), rng);
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
	EXPECT_EQ(dataset, result);
}

TEST(CsvTuple, MissingValues) {
	typedef discovery::csv_tuple<',',uint32_t,uint32_t,uint32_t> mytuple;
	typedef std::tuple<uint32_t,uint32_t,uint32_t> value_type;
	std::stringstream stream;
	stream << "1,,2\n,,4\n1,,";
	std::vector<value_type> result;
	std::copy(
		std::istream_iterator<mytuple>(stream),
		std::istream_iterator<mytuple>(),
		std::back_inserter(result)
	);
	EXPECT_EQ(3u, result.size());
	// first line
	EXPECT_EQ(1u, std::get<0>(result[0])) << "bad first line";
	EXPECT_EQ(0u, std::get<1>(result[0])) << "bad first line";
	EXPECT_EQ(2u, std::get<2>(result[0])) << "bad first line";
	// second line
	EXPECT_EQ(0u, std::get<0>(result[1])) << "bad second line";
	EXPECT_EQ(0u, std::get<1>(result[1])) << "bad second line";
	EXPECT_EQ(4u, std::get<2>(result[1])) << "bad second line";
	// third line
	EXPECT_EQ(1u, std::get<0>(result[2])) << "bad third line";
	EXPECT_EQ(0u, std::get<1>(result[2])) << "bad third line";
	EXPECT_EQ(0u, std::get<2>(result[2])) << "bad third line";
}
