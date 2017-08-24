#include "application.hh"

#include <ostream>
#include <unistdx/base/make_object>

std::ostream&
factory::operator<<(std::ostream& out, const Application& rhs) {
	return out << sys::make_object("exec", rhs._execpath);
}

