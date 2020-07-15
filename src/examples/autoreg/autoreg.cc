#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>

#include "autoreg_app.hh"

int main(int argc, char** argv) {
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Autoreg_app>(1);
        factory.types().add<autoreg::Autoreg_model<Real>>(2);
        factory.types().add<autoreg::Generator1<Real,autoreg::Uniform_grid>>(3);
        factory.types().add<autoreg::Wave_surface_generator<Real,autoreg::Uniform_grid>>(4);
    }
    factory_guard g;
    send(sbn::make_pointer<Autoreg_app>());
    return wait_and_return();
}
