#ifndef SUBORDINATION_CORE_EXIT_CODE_HH
#define SUBORDINATION_CORE_EXIT_CODE_HH

#include <iosfwd>

#include <unistdx/base/types>

namespace sbn {

    enum class exit_code: sys::u16 {
        success = 0,
        undefined = 1,
        error = 2,
        endpoint_not_connected = 3,
        no_principal_found = 4,
        no_upstream_servers_available = 5
    };

    const char* to_string(exit_code rhs) noexcept;
    std::ostream& operator<<(std::ostream& out, exit_code rhs);

}

#endif // vim:filetype=cpp
