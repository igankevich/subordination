#include <subordination/core/exit_code.hh>

#include <array>

namespace {
    std::array<const char*,6> all_exit_codes{
        "success",
        "undefined",
        "error",
        "endpoint_not_connected",
        "no_principal_found",
        "no_upstream_servers_available",
    };

}

const char*
sbn::to_string(exit_code rhs) noexcept {
    const exit_code_type i = exit_code_type(rhs);
    const char* str = "unknown";
    if (i < all_exit_codes.size()) {
        str = all_exit_codes[i];
    }
    return str;
}
