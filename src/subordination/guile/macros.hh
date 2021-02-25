#ifndef SUBORDINATION_GUILE_MACROS_HH
#define SUBORDINATION_GUILE_MACROS_HH

#include <array>
#include <ios>
#include <stdexcept>

namespace {

    inline void
    throw_error(const char* message) {
        scm_throw(scm_from_utf8_symbol("sbn-error"),
                  scm_list_1(scm_from_utf8_string(message)));
    }

    template <int Req, int Opt, int Rest, class Pointer> inline void
    define_procedure(const char *name,
                     std::array<const char*,Req> req,
                     std::array<const char*,Opt> opt,
                     std::array<const char*,Rest> rest,
                     const char* documentation, Pointer ptr) {
        SCM proc = scm_c_define_gsubr(name, Req, Opt, Rest==0?0:1,
                                      reinterpret_cast<scm_t_subr>(ptr));
        scm_set_procedure_property_x(proc, scm_from_utf8_symbol("documentation"),
                                     scm_from_utf8_string(documentation));
        SCM required = SCM_EOL;
        for (int i=Req-1; i>=0; --i) {
            required = scm_cons(scm_from_utf8_symbol(req[i]), required);
        }
        SCM optional = SCM_EOL;
        for (int i=Opt-1; i>=0; --i) {
            optional = scm_cons(scm_from_utf8_symbol(opt[i]), optional);
        }
        SCM keyword = SCM_EOL;
        for (int i=Rest-1; i>=0; --i) {
            keyword = scm_cons(scm_cons(scm_from_utf8_keyword(rest[i]), scm_from_int32(1)),
                               keyword);
        }
        scm_set_procedure_property_x(proc, scm_from_utf8_symbol("arglist"),
                                     scm_list_n(required, optional, keyword,
                                                scm_from_bool(false), scm_from_bool(false),
                                                SCM_UNDEFINED));
        scm_c_export(name, nullptr);
    }

    template <int Req, int Opt, class Pointer> inline void
    define_procedure(const char *name,
                     std::array<const char*,Req> req,
                     std::array<const char*,Opt> opt,
                     const char* documentation, Pointer ptr) {
        define_procedure<Req,Opt,0,Pointer>(name, req, opt, {}, documentation, ptr);
    }

    template <int Req, class Pointer> inline void
    define_procedure(const char *name,
                     std::array<const char*,Req> req,
                     const char* documentation, Pointer ptr) {
        define_procedure<Req,0,0,Pointer>(name, req, {}, {}, documentation, ptr);
    }

    template <class Pointer> inline void
    define_procedure(const char *name, const char* documentation, Pointer ptr) {
        define_procedure<0,0,0,Pointer>(name, {}, {}, {}, documentation, ptr);
    }

}

#define SBN_GUILE_CALL(func, ...) \
    try { \
        return func(__VA_ARGS__); \
    } catch (const ::std::ios::failure&) { \
        ::throw_error("i/o error"); \
    } catch (const ::std::exception& err) { \
        ::throw_error(err.what()); \
    } catch (...) { \
        ::throw_error("unknown error"); \
    } \
    return SCM_UNSPECIFIED

#define SBN_GUILE_0(func) \
    static_cast<SCM(*)()>([] () -> SCM { \
        SBN_GUILE_CALL(func); \
    })

#define SBN_GUILE_1(func) \
    static_cast<SCM(*)(SCM)>([] (SCM a) -> SCM { \
        SBN_GUILE_CALL(func,a); \
    })

#define SBN_GUILE_2(func) \
    static_cast<SCM(*)(SCM,SCM)>([] (SCM a, SCM b) -> SCM { \
        SBN_GUILE_CALL(func,a,b); \
    })

#define SBN_GUILE_3(func) \
    static_cast<SCM(*)(SCM,SCM,SCM)>([] (SCM a, SCM b, SCM c) -> SCM { \
        SBN_GUILE_CALL(func,a,b,c); \
    })

#define SBN_GUILE_4(func) \
    static_cast<SCM(*)(SCM,SCM,SCM,SCM)>([] (SCM a, SCM b, SCM c, SCM d) -> SCM { \
        SBN_GUILE_CALL(func,a,b,c,d); \
    })

#define SBN_GUILE_5(func) \
    static_cast<SCM(*)(SCM,SCM,SCM,SCM,SCM)>([] (SCM a, SCM b, SCM c, SCM d, SCM e) -> SCM { \
        SBN_GUILE_CALL(func,a,b,c,d,e); \
    })

#define SBN_GUILE_6(func) \
    static_cast<SCM(*)(SCM,SCM,SCM,SCM,SCM,SCM)>([] (SCM a, SCM b, SCM c, SCM d, SCM e, SCM f) -> SCM { \
        SBN_GUILE_CALL(func,a,b,c,d,e,f); \
    })

#endif // vim:filetype=cpp
