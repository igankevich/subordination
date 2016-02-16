#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>

#include <test.hh>

#include <sysx/endpoint.hh>

#include "csv_tuple.hh"
#include "network.hh"
#include "location.hh"

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
		test::compare_bytes(result, dataset);
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

template<class Addr>
struct Test_location: public test::Test<Test_location<Addr>> {

	typedef Addr addr_type;
	typedef discovery::Network<addr_type> network_type;
	typedef typename network_type::rep_type uint_type;
	typedef float float_type;
	typedef uint32_t city_type;
	typedef uint32_t country_type;
	typedef std::unordered_map<city_type,country_type> countrymap_type;

	template<class T, class Iterator>
	static T
	encode_as_number(Iterator first, Iterator last) {
		constexpr const T radix('Z' - 'A');
		T mult(1);
		T result(0);
		while (first != last) {
			result += T(std::toupper(*first)) * mult;
			mult *= radix;
			++first;
		}
		return result;
	}

	void
	load_countries(countrymap_type& countries) {
		using namespace discovery;
		typedef csv_tuple<',',
			city_type,
			ignore_field,
			ignore_field,
			ignore_field,
			std::string
		> mytuple;
		std::ifstream in("GeoLite2-City-Locations-en.csv");
		in.ignore(1024*1024, '\n');
		std::for_each(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			[&countries] (const mytuple& rhs) {
				const city_type city = std::get<0>(rhs);
				const std::string country_str = std::get<4>(rhs);
				if (country_str == "RU") {
					const country_type country = encode_as_number<country_type>(country_str.begin(), country_str.end());
					countries.emplace(city, country);
//					std::clog << country_str << std::endl;
				}
			}
		);
		std::clog << "No. of cities = " << countries.size() << std::endl;
	}

	void xrun() override {
		using namespace discovery;
//		typedef std::tuple<country_type,city_type,float_type,float_type> location_tuple;
		typedef std::set<City> locations_type;
		typedef std::unordered_map<city_type,uint_type> hostmap_type;
		typedef std::multiset<Location> locationset_type;
		typedef std::unordered_map<country_type,locationset_type> locationmap_type;
		typedef csv_tuple<',',
			network_type,
			city_type,
			country_type,
			ignore_field,
			ignore_field,
			ignore_field,
			ignore_field,
			float_type,
			float_type
		> mytuple;
		hostmap_type nhosts;
		locations_type locs;
		countrymap_type countries;
		load_countries(countries);
		std::ifstream in("GeoLite2-City-Blocks-IPv4.csv");
		in.ignore(1024*1024, '\n');
		std::for_each(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			[&locs,&nhosts,&countries] (const mytuple& rhs) {
				const network_type net = std::get<0>(rhs);
				const city_type city = std::get<1>(rhs);
				const float_type lat = std::get<7>(rhs);
				const float_type lon = std::get<8>(rhs);
				const country_type country = countries[city];
				if (city and country) {
					nhosts[city] += net.count();
					locs.emplace(City{country, city, lat, lon});
				}
			}
		);
		std::clog << "nhosts.size = " << nhosts.size() << std::endl;
		std::clog << "locs.size = " << locs.size() << std::endl;
		locationmap_type sorted_locs;
		std::for_each(
			locs.begin(),
			locs.end(),
			[&sorted_locs,&nhosts] (const City& rhs) {
//				const country_type country = std::get<0>(rhs);
//				const city_type city = std::get<1>(rhs);
//				const float_type lat = std::get<2>(rhs);
//				const float_type lon = std::get<3>(rhs);
				sorted_locs[rhs._country].emplace(rhs._city, rhs._latitude, rhs._longitude, nhosts[rhs._city]);
			}
		);

		std::ofstream vectors("vectors.dat");
		std::ofstream graph("graph.dat");
		std::for_each(
			sorted_locs.begin(),
			sorted_locs.end(),
			[&graph,&vectors] (const typename locationmap_type::value_type& rhs) {
				std::clog << "sorted_locs.size = " << rhs.second.size() << std::endl;
				const auto last = rhs.second.end();
				for (auto it=rhs.second.begin(); it!=last; ++it) {
					const Location& loc_a = *it;
					auto first = it;
					++first;
//					auto first = rhs.second.begin();
					auto result = std::min_element(
						first,
						last,
						[&loc_a] (const Location& lhs, const Location& rhs) {
							return lhs != loc_a and discovery::distance(loc_a, lhs)
								< discovery::distance(loc_a, rhs);
						}
					);
					const Location& loc_b = *result;
					if (result != last) {
						graph << loc_a << '\n' << loc_b << "\n\n\n";
						vectors << loc_a.point() << ' ' << (loc_b.point() - loc_a.point())*float_type(2)/float_type(3) << '\n';
					}
				}
			}
		);
	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test CSV", {
		new Test_synthetic,
		new Test_missing_values
//		new Test_location<sysx::ipv4_addr>
	}};
	return tests.run();
}
