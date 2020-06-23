#include <array>
#include <ostream>
#include <type_traits>

#include <subordination/core/kernel_base.hh>

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
    using exit_code_type = std::underlying_type<exit_code>::type;
    const auto i = exit_code_type(rhs);
    const char* str = "unknown";
    if (i < all_exit_codes.size()) {
        str = all_exit_codes[i];
    }
    return str;
}

std::ostream&
sbn::operator<<(std::ostream& out, exit_code rhs) {
    return out << to_string(rhs);
}
