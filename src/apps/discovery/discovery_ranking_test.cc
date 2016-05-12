#include <stdx/debug.hh>

#include <vector>
#include <iterator>
#include <map>
#include <algorithm>
#include <random>

#include <stdx/random.hh>
#include <sys/ifaddr_list.hh>
#include <test.hh>

#include "distance.hh"


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
	typedef sys::ifaddr<addr_type> network_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef typename network_type::rep_type rep_type;
	typedef std::random_device engine_type;

	void xrun() override {
		debug_addrs();
		test_addr_calculus();
		test_io();
		std::vector<network_type> networks;
		sys::enumerate_ifaddrs<sys::ipv4_addr>(std::back_inserter(networks));
		std::copy(
			networks.begin(), networks.end(),
			std::ostream_iterator<network_type>(std::clog, "\n")
		);
		test_ranking_algorithm({network_type(addr_type{127,0,0,1}, addr_type{255,0,0,0})}, 2, addr_type{});
		test_ranking_algorithm({network_type(addr_type{127,0,0,2}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,3}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,1});
		test_ranking_algorithm({network_type(addr_type{127,0,0,4}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,2});
		test_ranking_algorithm({network_type(addr_type{127,0,0,5}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,2});
		test_ranking_algorithm({network_type(addr_type{127,0,0,6}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,3});
		test_ranking_algorithm({network_type(addr_type{127,0,0,7}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,3});
		test_ranking_algorithm({network_type(addr_type{127,0,0,8}, addr_type{255,0,0,0})}, 2, addr_type{127,0,0,4});

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
//				const addr_type from(rhs.address());
//				const rep_type end = sys::to_host_format(from.rep());
//				for (rep_type i=rhs.start(); i<end; ++i) {
//					const addr_type to(sys::to_network_format(i));
//					const distance_type dist = distance_type(from, to, rhs.netmask(), fanout);
//					ranked_hosts.emplace(dist, to);
//				}
				std::transform(
					rhs.begin(), rhs.middle(),
					std::inserter(ranked_hosts, ranked_hosts.end()),
					[&rhs,fanout] (const addr_type& to) {
						const distance_type dist = distance_type(rhs.address(), to, rhs.netmask(), fanout);
						return std::make_pair(dist, to);
					}
				);
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
		typedef sys::ipv4_addr::rep_type rep_type;
		test::equal(sys::ipv4_addr{127,0,0,1}.position(sys::ipv4_addr{255,0,0,0}), rep_type(1));
		test::equal(sys::ipv4_addr{127,0,0,5}.position(sys::ipv4_addr{255,0,0,0}), rep_type(5));
		test::equal(distance_type(sys::ipv4_addr{127,0,0,2}, sys::ipv4_addr{127,0,0,1}, sys::ipv4_addr{255,0,0,0}, 2), distance_type(1, 0));
		test::equal(network_type(addr_type{127,0,0,7}, addr_type{255,0,0,0}).is_loopback(), true);
		test::equal(network_type(addr_type{128,0,0,7}, addr_type{255,0,0,0}).is_loopback(), false);
		test::equal(addr_type{255,255,255,0}.to_prefix(), sys::prefix_type(24));
		test::equal(addr_type{255,255,0,0}.to_prefix(), sys::prefix_type(16));
		test::equal(addr_type{255,0,0,0}.to_prefix(), sys::prefix_type(8));
	}

	void
	debug_addrs() {
		std::clog << distance_type(2, sys::ipv4_addr{127,0,0,1}, sys::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sys::ipv4_addr{127,0,0,2}, sys::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sys::ipv4_addr{127,0,0,3}, sys::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog << distance_type(2, sys::ipv4_addr{127,0,0,4}, sys::ipv4_addr{255,0,0,0}) << std::endl;
		std::clog
			<< std::setw(20) << "address"
			<< std::setw(20) << "position"
			<< std::setw(20) << "position in tree"
			<< std::endl;
		std::clog << std::right;
		for (unsigned char i=1; i<23; ++i) {
			const sys::ipv4_addr addr{127,0,0,i};
			std::clog
				<< std::setw(20) << addr
				<< std::setw(20) << addr.position(sys::ipv4_addr{255,0,0,0})
				<< std::setw(20) << distance_type(2, addr, sys::ipv4_addr{255,0,0,0}) << std::endl;
		}
	}

	void
	test_io() {
		network_type loopback({127,0,0,1}, 8);
		test::io_single(loopback);
		test::io_multiple<network_type>(std::bind(&Test_network::random_network, this));
	}

private:

	addr_type
	random_addr() {
		return addr_type(stdx::n_random_bytes<rep_type>(generator));
	}

	sys::prefix_type
	random_prefix() {
		static std::default_random_engine generator(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()
		);
		std::uniform_int_distribution<sys::prefix_type> dist(0, 32);
		auto gen = std::bind(dist, generator);
		return gen();
	}

	network_type
	random_network() {
		return network_type(random_addr(), random_prefix());
	}

	engine_type generator;
};


int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test network", {
		new Test_network<sys::ipv4_addr>
	}};
	return tests.run();
}
