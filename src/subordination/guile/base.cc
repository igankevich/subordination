#include <memory>

#include <subordination/guile/base.hh>

namespace  {
    struct c_deleter {
        inline void operator()(void* ptr) { std::free(ptr); }
    };
    using c_string = std::unique_ptr<char,c_deleter>;
}

std::string sbn::guile::object_to_string(SCM scm) {
    SCM port = scm_open_output_string();
    scm_write(scm, port);
    c_string ptr(scm_to_utf8_string(scm_get_output_string(port)));
    scm_close(port);
    return std::string(ptr.get());
}

SCM sbn::guile::string_to_object(const std::string& s) {
    SCM port = scm_open_input_string(scm_from_utf8_string(s.data()));
    SCM obj = scm_read(port);
    scm_close(port);
    return obj;
}
