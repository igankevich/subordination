#ifndef APPS_FACTORY_MASTER_DISCOVERER_HH
#define APPS_FACTORY_MASTER_DISCOVERER_HH

#include <chrono>
#include <iosfwd>

#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <factory/api.hh>
#include <factory/ppl/socket_pipeline_event.hh>

#include "hierarchy.hh"
#include "probe.hh"
#include "prober.hh"
#include "resident_kernel.hh"
#include "tree_hierarchy_iterator.hh"

namespace factory {

	/// Timer which is used to periodically scan nodes
	/// to find the best principal node.
	class discovery_timer: public factory::api::Kernel {};

	enum class probe_result {
		add_subordinate = 0,
		remove_subordinate,
		reject_subordinate,
		retain
	};

	std::ostream&
	operator<<(std::ostream& out, probe_result rhs);

	class master_discoverer: public resident_kernel {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef addr_type::rep_type uint_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;
		typedef tree_hierarchy_iterator<addr_type> iterator;
		typedef hierarchy<addr_type> hierarchy_type;
		typedef std::chrono::system_clock clock_type;
		typedef clock_type::duration duration;

		enum class state_type {
			initial,
			waiting,
			probing
		};

	private:
		/// Time period between subsequent network scans.
		duration _interval = std::chrono::minutes(1);
		uint_type _fanout = 10000;
		hierarchy_type _hierarchy;
		iterator _iterator, _end;
		discovery_timer* _timer = nullptr;
		state_type _state = state_type::initial;

	public:
		inline
		master_discoverer(const ifaddr_type& ifaddr, const sys::port_type port):
		_hierarchy(ifaddr, port),
		_iterator(ifaddr, this->_fanout)
		{}

		void
		on_start() override;

		void
		on_kernel(factory::api::Kernel* k) override;

	private:

		const ifaddr_type&
		ifaddr() const noexcept {
			return this->_hierarchy.ifaddr();
		}

		sys::port_type
		port() const noexcept {
			return this->_hierarchy.port();
		}

		void
		probe_next_node();

		void
		send_timer();

		void
		update_subordinates(probe* p);

		probe_result
		process_probe(probe* p);

		void
		update_principal(prober* p);

		inline void
		setstate(state_type rhs) noexcept {
			this->_state = rhs;
		}

		inline state_type
		state() const noexcept {
			return this->_state;
		}

		void
		on_event(socket_pipeline_kernel* k);

		void
		on_client_add(const sys::endpoint& endp);

		void
		on_client_remove(const sys::endpoint& endp);

		void
		propagate_hierarchy_state();

		void
		send_hierarchy(const sys::endpoint& dest);

	};


}

#endif // vim:filetype=cpp
