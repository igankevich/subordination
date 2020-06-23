#include <istream>
#include <ostream>

#include <dtest/exit_code.hh>

auto dts::to_string(exit_code rhs) -> std::string {
    switch (rhs) {
        case exit_code::master: return "master";
        case exit_code::all: return "all";
        case exit_code::none: return "none";
        default: return std::to_string(int(rhs)+1);
    }
}

std::ostream& dts::operator<<(std::ostream& out, exit_code rhs) {
    return out << to_string(rhs);
}

auto dts::to_exit_code(const std::string& s) -> exit_code {
    exit_code ret{};
    if (s == "master") { ret = exit_code::master; }
    else if (s == "all") { ret = exit_code::all; }
    else if (s == "none") { ret = exit_code::none; }
    else { ret = exit_code(std::stoi(s)-1); }
    return ret;
}

std::istream& dts::operator>>(std::istream& in, exit_code& rhs) {
    std::string tmp; in >> tmp;
    rhs = to_exit_code(tmp);
    return in;
}
