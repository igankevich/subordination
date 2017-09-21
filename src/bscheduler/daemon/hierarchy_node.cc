#include "hierarchy_node.hh"

#include <unistdx/base/make_object>

std::ostream&
bsc::operator<<(std::ostream& out, const hierarchy_node& rhs) {
	return out << sys::make_object(
		"endpoint", rhs.endpoint(),
		"weight", rhs.weight()
	);
}

