#include "network_master.hh"

#include <iterator>
#include <algorithm>

#include <unistdx/base/log_message>
#include <unistdx/it/field_iterator>
#include <unistdx/it/intersperse_iterator>
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

	template <class K, class V>
	std::ostream&
	operator<<(
		std::ostream& out,
		const std::unordered_map<K,V>& rhs
	) {
		typedef typename std::unordered_map<K,V>::const_iterator iterator;
		typedef sys::field_iterator<iterator,0> iter;
		copy(
			iter(rhs.begin()),
			iter(rhs.end()),
			sys::intersperse_iterator<K,char>(out, ',')
		);
		return out;
	}

}

namespace {

	template <class T, class X>
	std::unordered_set<T>
	set_difference_copy(
		const std::unordered_map<T,X>& lhs,
		const std::unordered_set<T>& rhs
	) {
		typedef typename std::unordered_map<T,X>::value_type value_type;
		std::unordered_set<T> result;
		for (const value_type& value : lhs) {
			if (rhs.find(value.first) == rhs.end()) {
				result.emplace(value.first);
			}
		}
		return result;
	}

	template <class T, class X>
	std::unordered_set<T>
	set_difference_copy(
		const std::unordered_set<T>& lhs,
		const std::unordered_map<T,X>& rhs
	) {
		std::unordered_set<T> result;
		for (const T& value : lhs) {
			if (rhs.find(value) == rhs.end()) {
				result.emplace(value);
			}
		}
		return result;
	}

}

void
factory::network_master::send_timer() {
	using namespace std::chrono;
	this->_timer->after(seconds(1));
	factory::api::send<>(this->_timer, this);
}

void
factory::network_master::act() {
	this->_timer = new network_timer;
	this->send_timer();
}

factory::network_master::set_type
factory::network_master::enumerate_ifaddrs() {
	set_type new_ifaddrs;
	sys::ifaddrs addrs;
	for (const sys::ifaddrs::value_type& ifa : addrs) {
		if (ifa.ifa_addr && ifa.ifa_addr->sa_family == traits_type::family) {
			ifaddr_type a(*ifa.ifa_addr, *ifa.ifa_netmask);
			if (!a.is_loopback() && !a.is_widearea()) {
				new_ifaddrs.emplace(a);
			}
		}
	}
	return new_ifaddrs;
}

void
factory::network_master::update_ifaddrs() {
	set_type new_ifaddrs = this->enumerate_ifaddrs();
	set_type ifaddrs_to_add = set_difference_copy(
		new_ifaddrs,
		this->_ifaddrs
	);
	set_type ifaddrs_to_rm = set_difference_copy(
		this->_ifaddrs,
		new_ifaddrs
	);
	// update servers in socket pipeline
	for (const ifaddr_type& ifaddr : ifaddrs_to_rm) {
		this->remove_ifaddr(ifaddr);
	}
	for (const ifaddr_type& ifaddr : ifaddrs_to_add) {
		this->add_ifaddr(ifaddr);
	}
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
factory::network_master::react(factory::api::Kernel* child) {
	if (child == this->_timer) {
		this->update_ifaddrs();
	} else if (typeid(*child) == typeid(probe)) {
		this->forward_probe(dynamic_cast<probe*>(child));
	}
}

void
factory::network_master::add_ifaddr(const ifaddr_type& ifa) {
	factory::factory.nic().add_server(ifa);
	if (this->_ifaddrs.find(ifa) == this->_ifaddrs.end()) {
		const sys::port_type port = ::factory::factory.nic().port();
		master_discoverer* d = new master_discoverer(ifa, port);
		this->_ifaddrs.emplace(ifa, d);
		factory::api::upstream(this, d);
	}
}

void
factory::network_master::remove_ifaddr(const ifaddr_type& ifa) {
	factory::factory.nic().remove_server(ifa);
	auto result = this->_ifaddrs.find(ifa);
	if (result != this->_ifaddrs.end()) {
		// the kernel is deleted automatically
		result->second->stop();
		this->_ifaddrs.erase(result);
	}
}

void
factory::network_master::forward_probe(probe* p) {
	auto result = this->_ifaddrs.find(p->ifaddr());
	if (result == this->_ifaddrs.end()) {
		sys::log_message("net", "bad probe _", p->ifaddr());
	} else {
		factory::api::send(p, result->second);
	}
}
