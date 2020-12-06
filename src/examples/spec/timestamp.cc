#include <istream>

#include <spec/timestamp.hh>

std::istream&
spec::operator>>(std::istream& in, Timestamp& rhs) {
    // YYYY MM DD hh mm
    // 2010 01 01 00 00
    // N.B. std::get_time is not thread-safe
    std::uint64_t minutes{};
    std::uint64_t hours{};
    std::uint64_t days{};
    std::uint64_t months{};
    std::uint64_t years{};
    in >> years;
    in >> months;
    in >> days;
    in >> hours;
    in >> minutes;
    rhs._timestamp = 60UL*(minutes + 60UL*(hours + 24UL*(days + 31UL*(months + 12UL*years))));
    return in;
}
