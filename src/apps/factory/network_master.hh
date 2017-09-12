#ifndef APPS_FACTORY_NETWORK_MASTER_HH
#define APPS_FACTORY_NETWORK_MASTER_HH

#include <factory/api.hh>
#include <unistdx/net/ifaddrs>
#include <unordered_set>
#include <unordered_map>

#include "master_discoverer.hh"

namespace factory {

	class network_timer: public factory::api::Kernel {};

	class network_master: public factory::api::Kernel {

	private:
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef typename sys::ipaddr_traits<addr_type> traits_type;
		typedef std::unordered_set<ifaddr_type> set_type;
		typedef std::unordered_map<ifaddr_type,master_discoverer*> map_type;

	private:
		map_type _ifaddrs;
		network_timer* _timer = nullptr;

	public:

		void
		act() override;

		void
		react(factory::api::Kernel* child) override;

	private:

		void
		send_timer();

		set_type
		enumerate_ifaddrs();

		void
		update_ifaddrs();

		void
		add_ifaddr(const ifaddr_type& rhs);

		void
		remove_ifaddr(const ifaddr_type& rhs);

		/// forward the probe to an appropriate discoverer
		void
		forward_probe(probe* p);

	};

}

#endif // vim:filetype=cpp
