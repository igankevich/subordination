#include <subordination/api.hh>
#include <subordination/base/error_handler.hh>
#include "autoreg_app.hh"

int
main(int argc, char** argv) {
    using namespace bsc;
    install_error_handler();
    types.register_type<autoreg::Autoreg_model<Real>>();
    types.register_type<autoreg::Generator1<Real,autoreg::Uniform_grid>>();
    types.register_type<autoreg::Wave_surface_generator<Real,autoreg::Uniform_grid>>();
    factory_guard g;
    if (this_application::is_master()) {
        send(new Autoreg_app);
    }
    return wait_and_return();
}
