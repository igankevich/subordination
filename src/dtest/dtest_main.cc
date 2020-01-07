#include <iostream>

#include <unistdx/ipc/signal>

#include <dtest/application.hh>

using s = sys::signal;
dts::application app;

void on_terminate(int sig) { app.send(s::terminate); ::alarm(3); }
void on_alarm(int) { app.send(s::kill); }

void
init_signal_handlers() {
    using namespace sys::this_process;
    ignore_signal(s::broken_pipe);
    bind_signal(s::keyboard_interrupt, on_terminate);
    bind_signal(s::terminate, on_terminate);
    bind_signal(s::alarm, on_alarm);
}

int main(int argc, char* argv[]) {
    int ret = EXIT_FAILURE;
    try {
        init_signal_handlers();
        app.init(argc, argv);
        app.run();
        ret = app.wait();
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        app.terminate();
    }
    return ret;
}
