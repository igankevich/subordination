#include <subordination/api.hh>
#include <subordination/base/error_handler.hh>
#include "autoreg_app.hh"

int
main(int argc, char** argv) {
    using namespace sbn;
    install_error_handler();
    types.register_type<autoreg::Autoreg_model<Real>>(1);
    types.register_type<autoreg::Generator1<Real,autoreg::Uniform_grid>>(2);
    types.register_type<autoreg::Wave_surface_generator<Real,autoreg::Uniform_grid>>(3);
    factory_guard g;
    if (this_application::is_master()) {
        send(new Autoreg_app);
    }
    return wait_and_return();
}
