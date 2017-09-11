#include "hierarchy.hh"

#include <algorithm>

#include <unistdx/net/ipv4_addr>
#include <unistdx/it/intersperse_iterator>

template <class Addr>
std::ostream&
factory::operator<<(std::ostream& out, const hierarchy<Addr>& rhs) {
	out << "ifaddr=" << rhs._ifaddr << ',';
	out << "principal=" << rhs._principal << ',';
	out << "subordinates=";
	std::copy(
		rhs._subordinates.begin(),
		rhs._subordinates.end(),
		sys::intersperse_iterator<sys::endpoint,char>(out, ',')
	);
	return out;
}

template class factory::hierarchy<sys::ipv4_addr>;

template std::ostream&
factory::operator<<(std::ostream& out, const hierarchy<sys::ipv4_addr>& rhs);
