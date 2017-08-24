#include "type_registry.hh"

#include <ostream>
#include <algorithm>

#include <factory/kernel/kernel_error.hh>

namespace {

	struct Entry {
		Entry(const factory::Type& rhs): _type(rhs) {}

		inline friend std::ostream&
		operator<<(std::ostream& out, const Entry& rhs) {
			return out << "/type/" << rhs._type.id() << '/' << rhs._type;
		}

	private:
		const factory::Type& _type;
	};

}

factory::Types::const_iterator
factory::Types::find(id_type id) const noexcept {
	return std::find_if(
		this->begin(), this->end(),
		[&id] (const Type& rhs) {
			return rhs.id() == id;
		}
	);
}

factory::Types::const_iterator
factory::Types::find(std::type_index idx) const noexcept {
	return std::find_if(
		this->begin(), this->end(),
		[&idx] (const Type& rhs) {
			return rhs.index() == idx;
		}
	);
}

void
factory::Types::register_type(Type type) {
	const_iterator result;
	result = this->find(type.index());
	if (result != this->end()) {
		FACTORY_THROW(Type_error, type, *result);
	}
	if (type) {
		result = this->find(type.id());
		if (result != this->end()) {
			FACTORY_THROW(Type_error, type, *result);
		}
	} else {
		type.setid(this->generate_id());
	}
	this->_types.emplace_back(type);
}

std::ostream&
factory::operator<<(std::ostream& out, const Types& rhs) {
	std::ostream_iterator<Entry> it(out, "\n");
	std::copy(rhs._types.begin(), rhs._types.end(), it);
	return out;
}

void*
factory::Types::read_object(sys::pstream& packet) {
	id_type id;
	packet >> id;
	const_iterator result = this->find(id);
	if (result == this->end()) {
		throw Kernel_error("unknown kernel type", id);
	}
	return result->read(packet);
}
