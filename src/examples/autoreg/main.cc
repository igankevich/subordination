#if defined(AUTOREG_MPI)

#include <unistdx/base/log_message>

#include <autoreg/autoreg_driver.hh>
#include <autoreg/mpi.hh>

int main(int argc, char* argv[]) {
    using T = AUTOREG_REAL_TYPE;
    autoreg::mpi::guard g(&argc, &argv);
    sys::log_message("autoreg", "_ _/_", autoreg::mpi::name, autoreg::mpi::rank+1,
                     autoreg::mpi::nranks);
    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto t = duration_cast<microseconds>(now);
    sys::log_message("autoreg", "xxx _", t.count());
    auto ptr = sbn::make_pointer<autoreg::Autoreg_model<T>>();
    if (argc >= 2) { ptr->filename(argv[1]); }
    send(std::move(ptr));
    return 0;
}

#else

#include <autoreg/api.hh>
#include <subordination/core/error_handler.hh>

#include <autoreg/autoreg_driver.hh>

int main(int argc, char** argv) {
    using namespace sbn;
    using T = AUTOREG_REAL_TYPE;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<autoreg::Autoreg_model<T>>(1);
        factory.types().add<autoreg::Wave_surface_generator<T>>(2);
        factory.types().add<autoreg::Part_generator<T>>(3);
    }
    factory_guard g;
    if (sbn::this_application::standalone()) {
        auto ptr = sbn::make_pointer<autoreg::Autoreg_model<T>>();
        if (argc >= 2) { ptr->filename(argv[1]); }
        send(std::move(ptr));
    }
    return wait_and_return();
}
#endif
