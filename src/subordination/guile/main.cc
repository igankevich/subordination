#include <libguile.h>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

void nested_main(int argc, char* argv[]) {
    using namespace sbn;
    using namespace sbn::guile;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        factory.types().add<Kernel>(2);
        factory.local().thread_init([] (size_t) { scm_init_guile(); });
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
