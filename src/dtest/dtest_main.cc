#include <iostream>

#include <unistdx/io/pipe>
#include <unistdx/ipc/process>
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
        sys::pipe pipe;
        pipe.in().unsetf(sys::open_flag::non_blocking);
        pipe.out().unsetf(sys::open_flag::non_blocking);
        using pf = sys::process_flag;
        sys::process child([&pipe] () {
            try {
                pipe.out().close();
                char ch;
                pipe.in().read(&ch, 1);
                app.run();
                return app.wait();
            } catch (const std::exception& err) {
                std::cerr << err.what() << std::endl;
                return 1;
            }
        }, pf::unshare_users | pf::unshare_network | pf::signal_parent);
        child.init_user_namespace();
        pipe.in().close();
        pipe.out().write("x", 1);
        ret = child.wait().exit_code();
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        app.terminate();
    }
    return ret;
}
