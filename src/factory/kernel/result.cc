#include "result.hh"

const char*
factory::to_string(factory::Result rhs) noexcept {
	switch (rhs) {
		case Result::success: return "success";
		case Result::undefined: return "undefined";
		case Result::endpoint_not_connected: return "endpoint_not_connected";
		case Result::no_principal_found: return "no_principal_found";
		case Result::error: return "error";
		default: return "unknown_result";
	}
}

