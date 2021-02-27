#include <libguile.h>

#include <sstream>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

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
    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv));
        //scm_c_primitive_load(argv[1]);
        //sbn::exit(0);
    }
    wait_and_return();
}

int main(int argc, char* argv[]) {
    sbn::guile::main(argc, argv, nested_main);
    return 0;
}
