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

        inline explicit properties(const char* filename) { open(filename); }
        inline explicit properties(std::istream& in, const char* filename) { read(in, filename); }
        void open(const char* filename);
        void read(std::istream& in, const char* filename);
        void read(int argc, const char** argv);
        inline void read(int argc, char** argv) { read(argc, const_cast<const char**>(argv)); }

        virtual void property(const std::string& key, const std::string& value) = 0;

    };

    bool string_to_bool(std::string s);
    bool starts_with(const std::string& s, const char* prefix) noexcept;

    class Duration: public std::chrono::system_clock::duration {
    public:
        using base_duration = std::chrono::system_clock::duration;
        using base_duration::duration;
        inline Duration(base_duration rhs) noexcept: base_duration(rhs) {}
    };

    auto string_to_duration(std::string s) -> Duration;
    std::istream& operator>>(std::istream& in, Duration& rhs);

}

#endif // vim:filetype=cpp
