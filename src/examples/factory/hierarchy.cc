#include "hierarchy.hh"

#include <algorithm>

#include <unistdx/net/ipv4_addr>
#include <unistdx/it/intersperse_iterator>

template <class Addr>
std::ostream&
asc::operator<<(std::ostream& out, const hierarchy<Addr>& rhs) {
	out << "ifaddr=" << rhs._ifaddr << ',';
	out << "principal=" << rhs._principal << ',';
	out << "subordinates=";
	std::copy(
		rhs._subordinates.begin(),
		rhs._subordinates.end(),
		sys::intersperse_iterator<hierarchy_node,char>(out, ',')
	);
	return out;
}

template <class Addr>
bool
asc::hierarchy<Addr>::set_subordinate_weight(
	const sys::endpoint& endp,
	weight_type w
) {
	bool changed = false;
	auto result = this->_subordinates.find(node_type(endp));
	if (result != this->_subordinates.end()) {
		weight_type old = result->weight();
		changed = old != w;
		if (changed) {
			result->weight(w);
		}
	}
	return changed;
}

template <class Addr>
typename asc::hierarchy<Addr>::weight_type
asc::hierarchy<Addr>::total_weight() const noexcept {
	// add 1 for the current node
	return this->principal_weight() + this->total_subordinate_weight() + 1;
}

template <class Addr>
typename asc::hierarchy<Addr>::weight_type
asc::hierarchy<Addr>::total_subordinate_weight() const noexcept {
	weight_type sum = 0;
	for (const node_type& n : this->_subordinates) {
		sum += n.weight();
	}
	return sum;
}

template class asc::hierarchy<sys::ipv4_addr>;

template std::ostream&
asc::operator<<(std::ostream& out, const hierarchy<sys::ipv4_addr>& rhs);
