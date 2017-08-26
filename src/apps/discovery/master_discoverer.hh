#ifndef APPS_DISCOVERY_MASTER_DISCOVERER_HH
#define APPS_DISCOVERY_MASTER_DISCOVERER_HH

#include "negotiator.hh"
#include "secret_agent.hh"
#include <factory/api.hh>

template<class Address, class Hierarchy, class Distance>
struct Master_discoverer: public Priority_kernel<Kernel> {

	typedef Address addr_type;
	typedef Hierarchy hierarchy_type;
	typedef Distance distance_type;
	typedef sys::ifaddr<addr_type> network_type;
	typedef std::multimap<distance_type,addr_type> rankedlist_type;
	typedef typename rankedlist_type::iterator rankedlist_iterator;
	typedef Negotiator<addr_type> negotiator_type;

	Master_discoverer(const network_type& network, const sys::port_type port):
	_hierarchy(network, port),
	_port(port),
	_rankedhosts(),
	_currenthost(),
	_negotiator(nullptr)
//	_cache(_hierarchy.this_addr(), _hierarchy),
	{
		generate_ranked_hosts(network);
	}

	Master_discoverer(const Master_discoverer&) = delete;
	Master_discoverer& operator=(const Master_discoverer&) = delete;

	void
	act() override {
		try_next_host();
	}

	void
	react(Kernel* k) override {
		if (_negotiator == k) {
			if (k->result() == Result::success) {
				_hierarchy.set_principal(_negotiator->new_principal());
				#ifndef NDEBUG
				sys::log_message("dscvr", "hierarchy=", _hierarchy);
				#endif
				_negotiator = nullptr;
				deploy_secret_agent();
			} else {
				try_next_host();
			}
		} else
		if (_secretagent == k) {
			#ifndef NDEBUG
			sys::log_message("dscvr", "secret agent returned from _, result _", k->from(), k->result());
			#endif
			if (k->result() == Result::endpoint_not_connected) {
				_hierarchy.unset_principal();
				#ifndef NDEBUG
				sys::log_message("dscvr", "hierarchy=", _hierarchy);
				#endif
				try_next_host();
			}
		} else
		if (typeid(*k) == typeid(negotiator_type)) {
			negotiator_type* neg = dynamic_cast<negotiator_type*>(k);
			neg->negotiate(_hierarchy);
			#ifndef NDEBUG
			sys::log_message("dscvr", "hierarchy=", _hierarchy);
			#endif
		}
	}

	const hierarchy_type&
	hierarchy() const noexcept {
		return _hierarchy;
	}

private:

	void
	generate_ranked_hosts(const network_type& rhs) {
		const auto fanout = 64;
		_rankedhosts.clear();
		std::transform(
			rhs.begin(), rhs.middle(),
			std::inserter(_rankedhosts, _rankedhosts.end()),
			[&rhs,fanout] (const addr_type& to) {
				const distance_type dist = distance_type(rhs.address(), to, rhs.netmask(), fanout);
				return std::make_pair(dist, to);
			}
		);
		_currenthost = _rankedhosts.end();
	}

	void
	try_next_host() {
		advance_through_ranked_list();
		run_negotiator();
	}

	void
	advance_through_ranked_list() {
		wrap_around();
		skip_this_host();
	}

	void
	wrap_around() {
		if (_currenthost == _rankedhosts.end()) {
			_currenthost = _rankedhosts.begin();
		} else {
			++_currenthost;
		}
	}

	void
	skip_this_host() {
		if (_currenthost != _rankedhosts.end() and _currenthost->second == _hierarchy.network().address()) {
			++_currenthost;
		}
	}

	void
	run_negotiator() {
		if (_currenthost != _rankedhosts.end()) {
			const sys::endpoint new_princ(_currenthost->second, _port);
			#ifndef NDEBUG
			sys::log_message("dscvr", "trying _", new_princ);
			#endif
			_negotiator = new Master_negotiator<addr_type>(_hierarchy.principal(), new_princ);
			upstream(local_server, this, _negotiator);
		}
	}

	void
	deploy_secret_agent() {
		using namespace factory::api;
		_secretagent = new Secret_agent;
		_secretagent->to(_hierarchy.principal());
//		_secretagent->id(factory()->factory_generate_id());
		_secretagent->parent(this);
		_secretagent->set_principal_id(_secretagent->to().address());
		send<Remote>(_secretagent);
	}

	hierarchy_type _hierarchy;
	sys::port_type _port;
	rankedlist_type _rankedhosts;
	rankedlist_iterator _currenthost;
//	discovery::Cache_guard<Peers> _cache;

	Master_negotiator<addr_type>* _negotiator;
	Secret_agent* _secretagent = nullptr;
};

#endif // APPS_DISCOVERY_MASTER_DISCOVERER_HH
