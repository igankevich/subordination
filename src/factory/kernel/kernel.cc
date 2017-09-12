#include "kernel.hh"

#include <factory/base/error.hh>
#include <unistdx/base/make_object>

namespace {

	inline factory::Kernel::id_type
	get_id(const factory::Kernel* rhs) {
		return !rhs ? factory::mobile_kernel::no_id() : rhs->id();
	}

}

void
factory::Kernel::read(sys::pstream& in) {
	base_kernel::read(in);
	bool b = false;
	in >> b;
	if (b) {
		this->setf(kernel_flag::carries_parent);
	}
	assert(not this->_parent);
	in >> this->_parent_id;
	assert(not this->_principal);
	in >> this->_principal_id;
}

void
factory::Kernel::write(sys::pstream& out) {
	base_kernel::write(out);
	out << carries_parent();
	if (this->moves_downstream()) {
		out << this->_parent_id << this->_principal_id;
	} else {
		if (this->isset(kernel_flag::parent_is_id)) {
			out << this->_parent_id;
		} else {
			out << get_id(this->_parent);
		}
		if (this->isset(kernel_flag::principal_is_id)) {
			out << this->_principal_id;
		} else {
			out << get_id(this->_principal);
		}
	}
}

void
factory::Kernel::act() {}

void
factory::Kernel::react(Kernel*) {
	FACTORY_THROW(Error, "empty react");
}

void
factory::Kernel::error(Kernel* rhs) {
	this->react(rhs);
}

std::ostream&
factory::operator<<(std::ostream& out, const Kernel& rhs) {
	const char state[] = {
		(rhs.moves_upstream()   ? 'u' : '-'),
		(rhs.moves_downstream() ? 'd' : '-'),
		(rhs.moves_somewhere()  ? 's' : '-'),
		(rhs.moves_everywhere() ? 'b' : '-'),
		0
	};
	return out << sys::make_object(
		"state", state,
		"type", typeid(rhs).name(),
		"id", rhs.id(),
		"src", rhs.from(),
		"dst", rhs.to(),
		"ret", rhs.result(),
		"app", rhs.app(),
		"parent", rhs._parent,
		"principal", rhs._principal
	);
}

