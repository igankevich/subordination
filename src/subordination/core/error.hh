#ifndef SUBORDINATION_CORE_ERROR_HH
#define SUBORDINATION_CORE_ERROR_HH

#include <array>
#include <exception>
#include <iosfwd>

namespace sbn {

    class error: public std::exception {

    private:
        const char* _what = nullptr;

    public:
        inline explicit error(const char* msg) noexcept: _what(msg) {}
        error() = default;
        virtual ~error() = default;
        error(const error&) = default;
        error& operator=(const error&) = default;
        error(error&&) = default;
        error& operator=(error&&) = default;

        virtual const char* what() const noexcept;
        virtual void write(std::ostream& out) const;

    };

    std::ostream& operator<<(std::ostream& out, const error& rhs);

    class type_error: public error {

    private:
        struct argument {

        public:
            enum type: unsigned char {
                type_none = 0,
                type_char = 1,
                type_short,
                type_int,
                type_long,
                type_long_long,
                type_unsigned_char,
                type_unsigned_short,
                type_unsigned_int,
                type_unsigned_long,
                type_unsigned_long_long,
                type_cstring,
            };

        private:
            union {
                char _char;
                short _short;
                int _int;
                long _long;
                long long _longlong;
                unsigned char _uchar;
                unsigned short _ushort;
                unsigned int _uint;
                unsigned long _ulong;
                unsigned long long _ulonglong;
                const char* _cstring;
            };
            type _type = type_none;

        public:
            inline argument(char a): _char(a), _type(type_char) {}
            inline argument(short a): _short(a), _type(type_short) {}
            inline argument(int a): _int(a), _type(type_int) {}
            inline argument(long a): _long(a), _type(type_long) {}
            inline argument(long long a): _longlong(a), _type(type_long_long) {}
            inline argument(unsigned char a): _uchar(a), _type(type_unsigned_char) {}
            inline argument(unsigned short a): _ushort(a), _type(type_unsigned_short) {}
            inline argument(unsigned int a): _uint(a), _type(type_unsigned_int) {}
            inline argument(unsigned long a): _ulong(a), _type(type_unsigned_long) {}
            inline argument(unsigned long long a): _ulonglong(a), _type(type_unsigned_long_long) {}
            inline argument(const char* a): _cstring(a), _type(type_cstring) {}
            inline argument(): _type(type_none) {}
            inline explicit operator bool() const noexcept { return this->_type != type_none; }
            void write(std::ostream& out) const;
        };

    private:
        std::array<argument,10> _args;

    public:
        template <class ... Arguments> inline
        type_error(const char* what, Arguments ... args):
        error(what), _args{args..., argument()} {
            static_assert(sizeof...(args) < 10, "bad no. of arguments");
        }

        void write(std::ostream& out) const override;

    };

}

#endif // vim:filetype=cpp
