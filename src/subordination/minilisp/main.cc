#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/minilisp/minilisp.hh>

int main(int argc, char** argv) {
    using namespace lisp;
    lisp::init();
    auto* env = lisp::top_environment();
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        //factory.local().num_upstream_threads(1);
        //factory.local().num_downstream_threads(0);
    }
    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv, env));
    }
    return wait_and_return();
}
