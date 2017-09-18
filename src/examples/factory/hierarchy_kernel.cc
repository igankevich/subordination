#include "hierarchy_kernel.hh"

void
factory::hierarchy_kernel::write(sys::pstream& out) {
	factory::api::Kernel::write(out);
	out << this->_weight;
}

void
factory::hierarchy_kernel::read(sys::pstream& in) {
	factory::api::Kernel::read(in);
	in >> this->_weight;
}

