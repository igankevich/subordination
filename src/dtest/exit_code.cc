#include <istream>
#include <ostream>

#include <dtest/exit_code.hh>

auto dts::to_string(exit_code rhs) -> const char* {
    switch (rhs) {
        case exit_code::master: return "master";
        case exit_code::all: return "all";
        default: return "unknown";
    }
}

std::ostream& dts::operator<<(std::ostream& out, exit_code rhs) {
    return out << to_string(rhs);
}

auto dts::to_exit_code(const std::string& s) -> exit_code {
    exit_code ret{};
    if (s == "master") { ret = exit_code::master; }
    else if (s == "all") { ret = exit_code::all; }
    return ret;
}

std::istream& dts::operator>>(std::istream& in, exit_code& rhs) {
    std::string tmp; in >> tmp;
    rhs = to_exit_code(tmp);
    return in;
}
