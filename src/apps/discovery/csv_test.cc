#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include <test.hh>

#include <sysx/endpoint.hh>

#include "csv_iterator.hh"
#include "network.hh"
#include "location.hh"

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

template<class Addr>
struct Test_location: public test::Test<Test_location<Addr>> {

	typedef Addr addr_type;
	typedef discovery::Network<addr_type> network_type;

	void xrun() override {
		using namespace discovery;
		typedef Location::id_type id_type;
		typedef Location::float_type float_type;
		typedef csv_tuple<',',
			network_type,
			id_type,
			ignore_field,
			ignore_field,
			ignore_field,
			ignore_field,
			ignore_field,
			float_type,
			float_type
		> mytuple;
		std::map<id_type,Location> locs;
		std::ifstream in("GeoLite2-City-Blocks-IPv4.csv");
		in.ignore(1024*1024, '\n');
		std::for_each(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			[&locs] (const mytuple& rhs) {
				const id_type id = std::get<1>(rhs);
				const float_type lat = std::get<7>(rhs);
				const float_type lon = std::get<8>(rhs);
				const network_type net = std::get<0>(rhs);
				auto result = locs.find(id);
				if (result == locs.end()) {
					locs.emplace(id, Location(id, lat, lon, net.count()));
				} else {
					result->second.add_hosts(net.count());
				}
			}
		);
//		std::transform(
//			locs.begin(),
//			locs.end(),
//			std::ostream_iterator<Location>(std::clog, "\n"),
//			[] (const std::map<id_type,Location>::value_type& rhs) {
//				return rhs.second;
//			}
//		);
		typedef const std::map<id_type,Location>::value_type& val_type;
//		for (auto it=locs.begin(); it!=locs.end(); ++it) {
//			std::cout << it->second << '\n';
//		}
		for (auto it=locs.begin(); it!=locs.end(); ++it) {
			Location loc_a = it->second;
			auto result = std::min_element(
				locs.begin(),
				locs.end(),
				[&loc_a] (val_type lhs, val_type rhs) {
					return lhs.second != loc_a and
					discovery::distance(loc_a, lhs.second)
						< discovery::distance(loc_a, rhs.second);
				}
			);
//			std::cout << it->first << " -- " << result->first << ";\n";
			std::cout << it->second << '\n' << result->second << "\n\n";
		}
	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test CSV", {
		new Test_csv<sysx::ipv4_addr>,
		new Test_location<sysx::ipv4_addr>
	}};
	return tests.run();
}
