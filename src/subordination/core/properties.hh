#ifndef SUBORDINATION_CORE_PROPERTIES_HH
#define SUBORDINATION_CORE_PROPERTIES_HH

namespace sbn {

    class properties {

    private:
        char _line_delimiter = '\n';
        char _word_delimiter = '=';
        char _comment_character = '#';

    public:

        properties() = default;
        virtual ~properties() = default;
        properties(const properties&) = delete;
        properties& operator=(const properties&) = delete;
        properties(properties&&) = delete;
        properties& operator=(properties&&) = delete;

        inline explicit properties(const char* filename) { read(filename); }
        inline explicit properties(std::istream& in, const char* filename) { read(in, filename); }
        void open(const char* filename);
        void read(std::istream& in, const char* filename);
        inline void read(const char* string) { read(1, const_cast<char**>(&string)); }
        void read(int argc, char** argv);
        virtual void property(const std::string& key, const std::string& value) = 0;

        inline void line_delimiter(char rhs) noexcept { this->_line_delimiter = rhs; }
        inline void word_delimiter(char rhs) noexcept { this->_word_delimiter = rhs; }
        inline void comment_character(char rhs) noexcept { this->_comment_character = rhs; }
        inline char line_delimiter() const noexcept { return this->_line_delimiter; }
        inline char word_delimiter() const noexcept { return this->_word_delimiter; }
        inline char comment_character() const noexcept { return this->_comment_character; }

    };

}

#endif // vim:filetype=cpp
