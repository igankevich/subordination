#include "type.hh"

#include <ostream>

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

std::ostream&
factory::operator<<(std::ostream& out, const Type& rhs) {
	return out << rhs.name() << '(' << rhs.id() << ')';
}

std::ostream&
factory::operator<<(std::ostream& out, const Type_error& rhs) {
	operator<<(out, static_cast<const Error&>(rhs));
	return out << " types '" << rhs._tp1 << "' and '" << rhs._tp2
			<< "' have the same type indices/identifiers";
}

const factory::Type*
factory::Types::lookup(id_type id) const noexcept {
	auto result = _types.find(Type(id));
	return result == _types.end() ? nullptr : &*result;
}

std::ostream&
factory::operator<<(std::ostream& out, const Types& rhs) {
	std::ostream_iterator<Entry> it(out, "\n");
	std::copy(rhs._types.begin(), rhs._types.end(), it);
	return out;
}

