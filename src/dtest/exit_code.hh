#ifndef DTEST_EXIT_CODE_HH
#define DTEST_EXIT_CODE_HH

#include <iosfwd>
#include <string>

namespace dts {

    enum struct exit_code: int {master=-1, all=-2, none=-3};

    auto to_string(exit_code rhs) -> std::string;
    auto to_exit_code(const std::string& s) -> exit_code;
    std::ostream& operator<<(std::ostream& out, exit_code rhs);
    std::istream& operator>>(std::istream& in, exit_code& rhs);

}

#endif // vim:filetype=cpp
