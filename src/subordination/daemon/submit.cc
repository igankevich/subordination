#include <iostream>
#include <string>
#include <vector>

#include <subordination/base/error_handler.hh>
#include <subordination/config.hh>
#include <subordination/daemon/small_factory.hh>
#include <subordination/ppl/application_kernel.hh>

using sbn::application_kernel;
using namespace sbn;

application_kernel*
new_application_kernel(int argc, char* argv[]) {
    typedef application_kernel::container_type container_type;
    container_type args, env;
    for (int i=1; i<argc; ++i) {
        args.emplace_back(argv[i]);
    }
    for (char** first=environ; *first; ++first) {
        env.emplace_back(*first);
    }
    application_kernel* k = new application_kernel(args, env);
    k->to(sys::socket_address(SUBORDINATION_UNIX_DOMAIN_SOCKET));
    return k;
}

class Main: public kernel {

private:
    int _argc;
    char** _argv;

public:
    Main(int argc, char** argv):
    _argc(argc),
    _argv(argv)
    {}

    void
    act() override {
        auto* k = new_application_kernel(this->_argc, this->_argv);
        k->parent(this);
        sbnc::factory.remote().send(k);
    }

    void
    react(kernel* child) override {
        auto* app = dynamic_cast<application_kernel*>(child);
        if (app->return_code() != sbn::exit_code::success) {
            std::string message = app->error();
            if (message.empty()) {
                message = to_string(app->return_code());
            }
            std::string app_id =
                app->application() == 0
                ? "application"
                : std::to_string(app->application());
            sys::log_message("submit", "failed to submit _: _", app_id, message);
        } else {
            sys::log_message("submit", "submitted _", app->application());
        }
        sbn::graceful_shutdown(int(app->return_code()));
    }

};

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        std::cerr << "Please, specify at least one argument." << std::endl;
        return 1;
    }
    sbn::install_error_handler();
    sbn::types.register_type<application_kernel>(1);
    try {
        sbnc::factory.start();
        sbnc::factory.remote().use_localhost(false);
        sbnc::factory.local().send(new Main(argc, argv));
    } catch (const std::exception& err) {
        sys::log_message("submit", "failed to connect to daemon process: _", err.what());
        sbn::graceful_shutdown(1);
    }
    auto ret = sbn::wait_and_return();
    sbnc::factory.stop();
    sbnc::factory.wait();
    return ret;
}
