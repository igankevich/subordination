#include <libguile.h>

#include <sstream>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

#include <subordination/guile/expression_kernel.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("guile", args...);
}

void print_pipeline_state(int sig) {
    using namespace sbn;
    {
        std::stringstream tmp;
        factory.write(tmp);
        log("pipeline state _", tmp.str());
    }
    std::_Exit(sig);
}

void nested_main(int argc, char* argv[]) {
    using namespace sbn;
    using namespace sbn::guile;
    install_error_handler();
    sys::this_process::bind_signal(sys::signal::quit, print_pipeline_state);
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        factory.types().add<Kernel>(2);
        factory.types().add<Map_kernel>(3);
        factory.types().add<Map_child_kernel>(4);
        factory.local().thread_init([] (size_t) { scm_init_guile(); });
        factory.remote().thread_init([] () { scm_init_guile(); });
    }
    {
        SCM ret = SCM_EOL;
        for (int i=argc-1; i>=0; --i) {
            ret = scm_cons(scm_from_utf8_string(argv[i]), ret);
        }
        scm_c_define("*global-arguments*", ret);
    }
    if (std::getenv("SBN_TEST_REFERENCE")) {
        scm_c_primitive_load(argv[1]);
        std::exit(0);
    }
    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv));
        //scm_c_primitive_load(argv[1]);
        //sbn::exit(0);
    }
    wait_and_return();
}

void nested_main_parallel(int argc, char* argv[]) {
using namespace sbn;
    using namespace sbn::guile;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<expression_kernel_main>(1);
        factory.types().add<expression_kernel>(2);
        factory.types().add<expression_kernel_if>(3);
        factory.types().add<expression_kernel_define>(4);
        factory.local().thread_init([] (size_t) { 
            scm_init_guile();

        });
    }

    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<expression_kernel_main>(argc, argv));
        //scm_c_primitive_load(argv[1]);
        //sbn::exit(0);
    }
    wait_and_return();
}

int main(int argc, char* argv[]) {
    sbn::guile::main(argc, argv, nested_main_parallel);
    return 0;
}
