#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>

#include <spec/spec_app.hh>

int
main(int argc, char** argv) {
    using T = SPECTRUM_REAL_TYPE;
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main<T>>(1);
        factory.types().add<Spectrum_directory_kernel<T>>(2);
        factory.types().add<Spectrum_file_kernel<T>>(3);
    }
    factory_guard g;
    if (sbn::this_application::standalone()) {
        send(sbn::make_pointer<Main<T>>(argc, argv));
    }
    return wait_and_return();
}
