#ifndef SUBORDINATION_GUILE_BASE_HH
#define SUBORDINATION_GUILE_BASE_HH

#include <libguile.h>

#include <string>

namespace sbn {

    namespace guile {

        auto object_to_string(SCM scm) -> std::string;
        auto string_to_object(const std::string& s) -> SCM;

        template <class T> T*
        allocate(const char* name) {
            return reinterpret_cast<T*>(scm_gc_malloc(sizeof(T), name));
        }

        template <class T, class ... Args> T*
        construct(const char* name, Args ... args) {
            auto* ptr = allocate<T>(name);
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }

        inline bool is_bound(SCM s) { return !SCM_UNBNDP(s); }
        inline bool symbol_equal(SCM a, SCM b) { return scm_is_true(scm_eq_p(a, b)); }

    }

}

#endif // vim:filetype=cpp
