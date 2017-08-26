#ifndef FACTORY_KERNEL_RESULT_HH
#define FACTORY_KERNEL_RESULT_HH

#include <unistdx/net/pstream>

namespace factory {

	typedef uint16_t result_type;

	enum struct Result: result_type {
		success = 0,
		undefined = 1,
		error = 2,
		endpoint_not_connected = 3,
		no_principal_found = 4
	};

	const char*
	to_string(factory::Result rhs) noexcept;

	inline std::ostream&
	operator<<(std::ostream& out, Result rhs) {
		return out << to_string(rhs);
	}

	inline sys::pstream&
	operator<<(sys::pstream& out, Result rhs) {
		return out << result_type(rhs);
	}

	inline sys::pstream&
	operator>>(sys::pstream& in, Result& rhs) {
		result_type tmp;
		in >> tmp;
		rhs = Result(tmp);
		return in;
	}

}

#endif // vim:filetype=cpp
