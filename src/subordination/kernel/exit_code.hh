#ifndef SUBORDINATION_KERNEL_EXIT_CODE_HH
#define SUBORDINATION_KERNEL_EXIT_CODE_HH

#include <unistdx/net/bstream>

namespace bsc {

    typedef uint16_t exit_code_type;

    enum struct exit_code: uint16_t {
        success = 0,
        undefined = 1,
        error = 2,
        endpoint_not_connected = 3,
        no_principal_found = 4,
        no_upstream_servers_available = 5
    };

    const char*
    to_string(exit_code rhs) noexcept;

    inline std::ostream&
    operator<<(std::ostream& out, exit_code rhs) {
        return out << to_string(rhs);
    }

    inline sys::bstream&
    operator<<(sys::bstream& out, exit_code rhs) {
        return out << exit_code_type(rhs);
    }

    inline sys::bstream&
    operator>>(sys::bstream& in, exit_code& rhs) {
        exit_code_type tmp;
        in >> tmp;
        rhs = exit_code(tmp);
        return in;
    }

}

#endif // vim:filetype=cpp
