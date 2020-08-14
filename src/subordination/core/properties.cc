#include <fstream>
#include <sstream>
#include <string>

#include <subordination/bits/string.hh>
#include <subordination/core/properties.hh>

using sbn::bits::trim_both;
using sbn::bits::trim_right;

namespace  {

    constexpr const char line_delimiter = '\n';
    constexpr const char word_delimiter = '=';
    constexpr const char comment_character = '#';

    enum State { Key, Value, Comment, Finished };

    inline bool parse(char ch, char line_delimiter, char word_delimiter,
                      char comment_character,
                      std::string& key, std::string& value, State& state, bool& success) {
        bool break_loop = false;
        if (state == Key) {
            if (ch == 0 || ch == line_delimiter) {
                success = false;
                break_loop = true;
            } else if (ch == comment_character) {
                state = Comment;
                success = false;
            } else if (ch == word_delimiter) {
                state = Value;
            } else {
                if (!(key.empty() && std::isspace(ch))) { key += ch; }
            }
        } else if (state == Value) {
            if (ch == 0 || ch == line_delimiter) {
                state = Finished;
                break_loop = true;
            } else if (ch == comment_character) {
                state = Comment;
            } else {
                if (!(value.empty() && std::isspace(ch))) { value += ch; }
            }
        } else if (state == Comment) {
            if (ch == 0 || ch == line_delimiter) {
                state = Finished;
                break_loop = true;
            }
        }
        return break_loop;
    }
}

void sbn::properties::open(const char* filename) {
    std::ifstream in;
    in.open(filename);
    if (!in.is_open()) {
        std::stringstream msg;
        msg << "failed to open \"" << filename << '\"';
        throw std::runtime_error(msg.str());
    }
    read(in, filename);
    in.close();
}

void sbn::properties::read(std::istream& in, const char* filename) {
    char ch;
    std::string key, value;
    key.reserve(512), value.reserve(512);
    for (int line_number=0; !in.eof(); ++line_number) {
        key.clear(), value.clear();
        State state = Key;
        bool success = true;
        while (true) {
            in.get(ch);
            if (!in.eof() && (in.rdstate() & (std::ios::failbit | std::ios::badbit))) {
                success = false;
                break;
            }
            if (in.eof()) { ch = 0; }
            if (parse(ch, line_delimiter, word_delimiter, comment_character,
                      key, value, state, success)) {
                break;
            }
        }
        if (success && state == Finished) {
            trim_right(key), trim_right(value);
            try {
                property(key, value);
            } catch (const std::exception& err) {
                std::stringstream msg;
                msg << filename << ':' << line_number << ": invalid \"" << key
                    << "\": " << err.what();
                throw std::runtime_error(msg.str());
            }
        } else if (!key.empty() || !value.empty()) {
            std::stringstream msg;
            msg << filename << ':' << line_number << ": invalid line, key=\"" << key
                << "\", value=\"" << value << '\"';
            throw std::runtime_error(msg.str());
        }
    }
}

void sbn::properties::read(int argc, char** argv) {
    std::string key, value;
    key.reserve(512), value.reserve(512);
    char ch = 0;
    for (int i=0; i<argc; ++i) {
        char* s = argv[i];
        do {
            key.clear(), value.clear();
            State state = Key;
            bool success = true;
            while (!parse(ch=*s++, ' ', word_delimiter, '\0',
                          key, value, state, success)) {}
            if (success && state == Finished) {
                trim_right(key), trim_right(value);
                property(key, value);
            } else if (!key.empty() || !value.empty()) {
                std::stringstream msg;
                msg << "invalid property: key=\"" << key << "\", value=\"" << value << '\"';
                throw std::runtime_error(msg.str());
            }
        } while (ch != 0);
    }
}

bool sbn::string_to_bool(std::string s) {
    trim_both(s);
    for (auto& ch : s) { ch = std::tolower(ch); }
    if (s == "yes" || s == "on" || s == "1") { return true; }
    if (s == "no" || s == "off" || s == "0") { return false; }
    throw std::invalid_argument("bad boolean");
}

auto sbn::string_to_duration(std::string s) -> Duration {
    using namespace std::chrono;
    using d = Duration::base_duration;
    using days = std::chrono::duration<Duration::rep,std::ratio<60*60*24>>;
    trim_both(s);
    std::size_t i = 0, n = s.size();
    Duration::rep value = std::stoul(s, &i);
    std::string suffix;
    if (i != n) { suffix = s.substr(i); }
    if (suffix == "ns") { return duration_cast<d>(nanoseconds(value)); }
    if (suffix == "us") { return duration_cast<d>(microseconds(value)); }
    if (suffix == "ms") { return duration_cast<d>(milliseconds(value)); }
    if (suffix == "s" || suffix.empty()) { return duration_cast<d>(seconds(value)); }
    if (suffix == "m") { return duration_cast<d>(minutes(value)); }
    if (suffix == "h") { return duration_cast<d>(hours(value)); }
    if (suffix == "d") { return duration_cast<d>(days(value)); }
    std::stringstream tmp;
    tmp << "unknown duration suffix \"" << suffix << "\"";
    throw std::invalid_argument(tmp.str());
}

std::istream& sbn::operator>>(std::istream& in, Duration& rhs) {
    std::string s; in >> s; rhs = string_to_duration(s); return in;
}
