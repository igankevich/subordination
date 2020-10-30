#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/python/init.hh>
#include <subordination/python/kernel.hh>


void nested_main(int argc, char* argv[]) {
    using namespace sbn;
    using namespace sbn::python;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        factory.types().add<kernel_map>(2);
    }
    factory_guard g;
    if (this_application::standalone())
        send(sbn::make_pointer<Main>(argc, argv));
    wait_and_return();
}

int main(int argc, char* argv[]) {
    sbn::python::main(argc, argv, nested_main);
    return 0;
}
