#ifndef FACTORY_RESULT_HH
#define FACTORY_RESULT_HH

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
	to_string(Result rhs) noexcept {
		switch (rhs) {
			case Result::success: return "success";
			case Result::undefined: return "undefined";
			case Result::endpoint_not_connected: return "endpoint_not_connected";
			case Result::no_principal_found: return "no_principal_found";
			case Result::error: return "error";
			default: return "unknown_result";
		}
	}

	std::ostream&
	operator<<(std::ostream& out, Result rhs) {
		return out << to_string(rhs);
	}

	sys::packetstream&
	operator<<(sys::packetstream& out, Result rhs) {
		return out << result_type(rhs);
	}

	sys::packetstream&
	operator>>(sys::packetstream& in, Result& rhs) {
		result_type tmp;
		in >> tmp;
		rhs = Result(tmp);
		return in;
	}

}

#endif // FACTORY_RESULT_HH
