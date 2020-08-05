#include <fstream>
#include <sstream>
#include <string>

#include <subordination/core/properties.hh>

namespace  {
    enum State { Key, Value, Comment, Finished };
    inline void trim_right(std::string& s) {
        while (!s.empty() && std::isspace(s.back())) { s.pop_back(); }
    }
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
            if (parse(ch, line_delimiter(), word_delimiter(), comment_character(),
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
            while (!parse(ch=*s++, line_delimiter(), word_delimiter(), comment_character(),
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
