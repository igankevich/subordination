#include "network_master.hh"

#include <iterator>
#include <algorithm>

#include <unistdx/base/log_message>
#include <unistdx/net/ifaddrs>

namespace std {
	template <class T>
	std::ostream&
	operator<<(
		std::ostream& out,
		const std::unordered_set<T>& rhs
	) {
		copy(
			rhs.begin(),
			rhs.end(),
			sys::intersperse_iterator<T,char>(out, ',')
		);
		return out;
	}
}

namespace {

	template <class T>
	std::unordered_set<T>
	set_difference_copy(
		const std::unordered_set<T>& lhs,
		const std::unordered_set<T>& rhs
	) {
		std::unordered_set<T> result;
		std::copy_if(
			lhs.begin(),
			lhs.end(),
			std::inserter(result, result.end()),
			[&rhs] (const T& value) {
				return rhs.find(value) == rhs.end();
			}
		);
		return result;
	}

	template <class T>
	void
	set_difference(
		std::unordered_set<T>& lhs,
		const std::unordered_set<T>& rhs
	) {
		for (const T& value : rhs) {
			lhs.erase(value);
		}
	}

}

void
factoryd::network_master::send_timer() {
	using namespace std::chrono;
	this->_timer->after(seconds(1));
	factory::api::send<>(this->_timer, this);
}

void
factoryd::network_master::act() {
	this->_timer = new network_timer;
	this->send_timer();
}

factoryd::network_master::set_type
factoryd::network_master::enumerate_ifaddrs() {
	set_type new_ifaddrs;
	sys::ifaddrs addrs;
	for (const sys::ifaddrs::value_type& ifa : addrs) {
		if (ifa.ifa_addr and ifa.ifa_addr->sa_family == traits_type::family) {
			ifaddr_type a(*ifa.ifa_addr, *ifa.ifa_netmask);
			if (!a.is_loopback() && !a.is_widearea()) {
				new_ifaddrs.emplace(a);
			}
		}
	}
	return new_ifaddrs;
}

void
factoryd::network_master::update_ifaddrs() {
	set_type new_ifaddrs = this->enumerate_ifaddrs();
	set_type ifaddrs_to_add = set_difference_copy(
		new_ifaddrs,
		this->_ifaddrs
	);
	set_type ifaddrs_to_rm = set_difference_copy(
		this->_ifaddrs,
		new_ifaddrs
	);
	this->_ifaddrs.insert(ifaddrs_to_add.begin(), ifaddrs_to_add.end());
	set_difference(this->_ifaddrs, ifaddrs_to_rm);
	sys::log_message(
		"net",
		"add=_ rm=_ actual=_",
		ifaddrs_to_add,
		ifaddrs_to_rm,
		this->_ifaddrs
	);
	this->send_timer();
}

void
factoryd::network_master::react(factory::api::Kernel* child) {
	if (child == this->_timer) {
		this->update_ifaddrs();
	}
}
