#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include "autoreg_app.hh"

int
main(int argc, char** argv) {
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<autoreg::Autoreg_model<Real>>(1);
        factory.types().add<autoreg::Generator1<Real,autoreg::Uniform_grid>>(2);
        factory.types().add<autoreg::Wave_surface_generator<Real,autoreg::Uniform_grid>>(3);
    }
    factory_guard g;
    if (this_application::is_master()) {
        send(new Autoreg_app);
    }
    return wait_and_return();
}
