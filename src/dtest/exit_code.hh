#ifndef DTEST_EXIT_CODE_HH
#define DTEST_EXIT_CODE_HH

#include <iosfwd>
#include <string>

namespace dts {

    enum struct exit_code {master, all};

    auto to_string(exit_code rhs) -> const char*;
    auto to_exit_code(const std::string& s) -> exit_code;
    std::ostream& operator<<(std::ostream& out, exit_code rhs);
    std::istream& operator>>(std::istream& in, exit_code& rhs);

}

#endif // vim:filetype=cpp
