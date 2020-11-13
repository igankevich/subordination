#ifndef SUBORDINATION_CORE_ERROR_HH
#define SUBORDINATION_CORE_ERROR_HH

#include <unistdx/system/error>

#include <array>
#include <exception>
#include <iosfwd>
#include <sstream>

namespace sbn {

    class error: public sys::error {

    public:
        template <class ... Arguments> inline
        error(const char* what, Arguments ... args):
        sys::error(make_message(what, args...).data()) {}

    private:

        static inline void print(std::ostream&) {}

        template <class Head, class ... Tail>
        static void print(std::ostream& out, const Head& head, const Tail& ... tail) {
            out << head;
            print(out, tail...);
        }

        template <class ... Args>
        static std::string make_message(const char* what, const Args& ... args) {
            std::stringstream tmp;
            tmp << what;
            print(tmp, args...);
            return tmp.str();
        }

    };

    inline void throw_error(const char* what) { throw error(what); }

    template <class ... Arguments>
    inline void throw_error(const char* what, Arguments ... args) {
        throw error(what, args...);
    }

}

#endif // vim:filetype=cpp
