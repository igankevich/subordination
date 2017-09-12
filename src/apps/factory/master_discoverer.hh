#ifndef APPS_FACTORY_MASTER_DISCOVERER_HH
#define APPS_FACTORY_MASTER_DISCOVERER_HH

#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <factory/api.hh>

#include "hierarchy.hh"
#include "tree_hierarchy_iterator.hh"

namespace factory {

	class master_discoverer: public factory::api::Kernel {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef tree_hierarchy_iterator<addr_type> iterator;
		typedef hierarchy<addr_type> hierarchy_type;

	private:
		hierarchy_type _hierarchy;
		iterator _iterator;

	public:
		inline
		master_discoverer(const ifaddr_type& ifaddr, const sys::port_type port):
		_hierarchy(ifaddr, port)
		{}

		void
		act() override;

		void
		react(factory::api::Kernel* k) override;

	};


}

#endif // vim:filetype=cpp
