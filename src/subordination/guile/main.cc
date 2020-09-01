#include <libguile.h>

#include <fstream>
#include <sstream>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

void nested_main(int argc, char* argv[]) {
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<sbn::guile::Main>(1);
        factory.types().add<sbn::guile::Kernel>(2);
    }
    factory_guard g;
    if (sbn::this_application::standalone()) {
        send(sbn::make_pointer<sbn::guile::Main>(argc, argv));
    }
    wait_and_return();
}

int main(int argc, char* argv[]) {
    sbn::guile::main(argc, argv, nested_main);
    return 0;
}
