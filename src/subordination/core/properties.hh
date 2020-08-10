#ifndef SUBORDINATION_CORE_PROPERTIES_HH
#define SUBORDINATION_CORE_PROPERTIES_HH

#include <chrono>
#include <iosfwd>
#include <string>

namespace sbn {

    class properties {

    public:

        properties() = default;
        virtual ~properties() = default;
        properties(const properties&) = default;
        properties& operator=(const properties&) = default;
        properties(properties&&) = default;
        properties& operator=(properties&&) = default;

        inline explicit properties(const char* filename) { read(filename); }
        inline explicit properties(std::istream& in, const char* filename) { read(in, filename); }
        void open(const char* filename);
        void read(std::istream& in, const char* filename);
        inline void read(const char* string) { read(1, const_cast<char**>(&string)); }
        void read(int argc, char** argv);
        virtual void property(const std::string& key, const std::string& value) = 0;

    };

    bool string_to_bool(std::string s);

    class Duration: public std::chrono::system_clock::duration {
    public:
        using base_duration = std::chrono::system_clock::duration;
        using base_duration::duration;
    };

    auto string_to_duration(std::string s) -> Duration;
    std::istream& operator>>(std::istream& in, Duration& rhs);

}

#endif // vim:filetype=cpp
