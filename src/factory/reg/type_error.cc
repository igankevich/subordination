#include "type_error.hh"

std::ostream&
factory::operator<<(std::ostream& out, const Type_error& rhs) {
	operator<<(out, static_cast<const Error&>(rhs));
	return out << rhs.what() << ": '" << rhs._tp1 << "' and '"
		<< rhs._tp2 << "'";
}

