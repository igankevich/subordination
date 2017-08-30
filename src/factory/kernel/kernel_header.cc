#include "kernel_header.hh"

#include <unistdx/base/make_object>
#include <ostream>

std::ostream&
factory::operator<<(std::ostream& out, const kernel_header& rhs) {
	return out << sys::make_object(
		"src", rhs._src,
		"dst", rhs._dst,
		"app", rhs._app
	);
}

