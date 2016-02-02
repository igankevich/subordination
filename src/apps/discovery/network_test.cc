#include <vector>
#include <iterator>
#include <map>
#include <algorithm>

#include "discovery.hh"
#include "distance.hh"

#include <test.hh>

namespace std {

	template<class X, class Y>
	std::ostream&
	operator<<(std::ostream& out, const std::pair<X,Y>& rhs) {
		return out << stdx::make_fields("rank", rhs.first, "addr", rhs.second);
	}

}

template<class Addr>
struct Test_network: public test::Test<Test_network<Addr>> {

	typedef Addr addr_type;
	typedef discovery::Network<addr_type> network_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef typename network_type::rep_type rep_type;

	void xrun() override {
		test_addr_calculus();
		std::vector<network_type> networks;
		discovery::enumerate_networks<sysx::ipv4_addr>(std::back_inserter(networks));
		std::copy(
			networks.begin(), networks.end(),
			std::ostream_iterator<network_type>(std::clog, "\n")
		);
		test_ranking_algorithm({network_type(addr_type{127,0,0,1}, addr_type{255,0,0,0})}, 2, addr_type{});
		test_ranking_algorithm({network_type(addr_type{127,0,0,2}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,3}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,4}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,2});
		test_ranking_algorithm({network_type(addr_type{127,0,0,4}, addr_type{255,0,0,0})}, 4, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,5}, addr_type{255,0,0,0})}, 4, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,6}, addr_type{255,0,0,0})}, 4, addr_type{127,0,0,2});
		test_ranking_algorithm({network_type(addr_type{127,0,0,7}, addr_type{255,0,0,0})}, 4, addr_type{127,0,0,2});
	}

	void
	test_ranking_algorithm(const std::initializer_list<network_type>& networks, const rep_type fanout, addr_type expected) {
		std::multimap<distance_type,addr_type> ranked_hosts;
		std::for_each(
			networks.begin(), networks.end(),
			[&ranked_hosts,fanout] (const network_type& rhs) {
//				if (!rhs.is_loopback() && !rhs.is_widearea()) {
					const addr_type from(rhs.address());
					const rep_type end = sysx::to_host_format(from.rep());
					for (rep_type i=rhs.start(); i<end; ++i) {
						const addr_type to(sysx::to_network_format(i));
						const distance_type dist = distance_type(from, to, rhs.netmask(), fanout);
						ranked_hosts.emplace(dist, to);
					}
//				}
			}
		);
		std::clog << "Expecting " << expected << " be the first in the following list." << std::endl;
		std::copy(
			ranked_hosts.begin(), ranked_hosts.end(),
			std::ostream_iterator<std::pair<const distance_type,addr_type>>(std::clog, "\n")
		);
		addr_type result = ranked_hosts.empty() ? addr_type{} : ranked_hosts.begin()->second;
		test::equal(result, expected, "ranking does not work", "fanout=", fanout);
	}

	void
	test_addr_calculus() {
		typedef sysx::ipv4_addr::rep_type rep_type;
		test::equal(sysx::ipv4_addr{127,0,0,1}.position(sysx::ipv4_addr{255,0,0,0}), rep_type(1));
		test::equal(sysx::ipv4_addr{127,0,0,5}.position(sysx::ipv4_addr{255,0,0,0}), rep_type(5));
		test::equal(distance_type(sysx::ipv4_addr{127,0,0,2}, sysx::ipv4_addr{127,0,0,1}, sysx::ipv4_addr{255,0,0,0}, 2), distance_type(1, 1));
	}

	void
	debug_addrs() {
		std::clog << distance_type(2, sysx::ipv4_addr{127,0,0,1}, sysx::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sysx::ipv4_addr{127,0,0,2}, sysx::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sysx::ipv4_addr{127,0,0,3}, sysx::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sysx::ipv4_addr{127,0,0,4}, sysx::ipv4_addr{255,0,0,0}) << std::endl;
		for (unsigned char i=1; i<23; ++i) {
			const sysx::ipv4_addr addr{127,0,0,i};
			std::clog << addr << "   " << distance_type(4, addr, sysx::ipv4_addr{255,0,0,0}) << std::endl;
		}
	}

};


int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test network", {
		new Test_network<sysx::ipv4_addr>
	}};
	return tests.run();
}
