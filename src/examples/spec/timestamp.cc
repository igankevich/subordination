#include <istream>

#include <spec/timestamp.hh>

std::istream&
spec::operator>>(std::istream& in, Timestamp& rhs) {
    // YYYY MM DD hh mm
    // 2010 01 01 00 00
    // N.B. std::get_time is not thread-safe
    std::tm t{};
    in >> t.tm_year;
    in >> t.tm_mon;
    in >> t.tm_mday;
    in >> t.tm_hour;
    in >> t.tm_min;
    rhs._timestamp = 60UL*(
        (std::uint64_t)t.tm_min + 60UL*(
            (std::uint64_t)t.tm_hour + 24UL*(
                (std::uint64_t)t.tm_mday + 31UL*(
                    (std::uint64_t)t.tm_mon + 12UL*(std::uint64_t)t.tm_year))));
    return in;
}

