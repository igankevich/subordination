#ifndef BSCHEDULER_DAEMON_MASTER_DISCOVERER_HH
#define BSCHEDULER_DAEMON_MASTER_DISCOVERER_HH

#include <chrono>
#include <iosfwd>

#include <unistdx/base/log_message>
#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

#include <bscheduler/api.hh>
#include <bscheduler/ppl/socket_pipeline_event.hh>

#include "hierarchy.hh"
#include "hierarchy_kernel.hh"
#include "probe.hh"
#include "prober.hh"
#include "resident_kernel.hh"
#include "tree_hierarchy_iterator.hh"


namespace bsc {

	/// Timer which is used to periodically scan nodes
	/// to find the best principal node.
	class discovery_timer: public bsc::kernel {};

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
		typedef typename hierarchy_type::weight_type weight_type;

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
		state_type _state = state_type::initial;

	public:
		inline
		master_discoverer(
			const ifaddr_type& ifaddr,
			const sys::port_type port,
			uint_type fanout
		):
		_fanout(fanout),
		_hierarchy(ifaddr, port),
		_iterator(ifaddr, fanout)
		{}

		void
		on_start() override;

		void
		on_kernel(bsc::kernel* k) override;

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
		broadcast_hierarchy(sys::endpoint ignored_endpoint = sys::endpoint());

		void
		send_weight(const sys::endpoint& dest, weight_type w);

		void
		update_weights(hierarchy_kernel* k);

		template <class ... Args>
		inline void
		log(const char* fmt, const Args& ... args) {
			#if defined(BSCHEDULER_PROFILE_NODE_DISCOVERY)
			using namespace std::chrono;
			const auto now = system_clock::now().time_since_epoch();
			const auto t = duration_cast<milliseconds>(now);
			std::string new_fmt;
			new_fmt += "[time since epoch _ms] ";
			new_fmt += fmt;
			sys::log_message("discoverer", new_fmt.data(), t.count(), args...);
			#else
			sys::log_message("discoverer", fmt, args ...);
			#endif
		}

	};


}

#endif // vim:filetype=cpp
