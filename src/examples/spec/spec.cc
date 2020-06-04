#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>

#include "spec_app.hh"

int
main(int argc, char** argv) {
    using namespace sbn;
    install_error_handler();
    factory_guard g;
    send(new Spec_app);
    return wait_and_return();
}
