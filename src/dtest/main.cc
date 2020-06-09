#include <iostream>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/io/pipe>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>
#include <unistdx/ipc/signal>

#include <dtest/application.hh>

using s = sys::signal;
dts::application* aptr;

void on_terminate(int sig) { if (aptr) { aptr->send(s::terminate); ::alarm(3); } }
void on_alarm(int) { if (aptr) { aptr->send(s::kill); } }

void
init_signal_handlers() {
    using namespace sys::this_process;
    ignore_signal(s::broken_pipe);
    bind_signal(s::keyboard_interrupt, on_terminate);
    bind_signal(s::terminate, on_terminate);
    bind_signal(s::alarm, on_alarm);
}

int nested_main(int argc, char* argv[], bool& will_restart) {
    int ret = EXIT_FAILURE;
    dts::application app;
    aptr = &app;
    try {
        init_signal_handlers();
        app.init(argc, argv);
        will_restart = app.will_restart();
        sys::pipe pipe;
        pipe.in().unsetf(sys::open_flag::non_blocking);
        pipe.out().unsetf(sys::open_flag::non_blocking);
        using pf = sys::process_flag;
        sys::process child([&pipe,&app] () {
            try {
                pipe.out().close();
                char ch;
                pipe.in().read(&ch, 1);
                using f = sys::unshare_flag;
                sys::this_process::unshare(f::users | f::network);
                app.run();
                return app.wait();
            } catch (const std::exception& err) {
                std::cerr << err.what() << std::endl;
                return 1;
            }
        //}, pf::unshare_users | pf::unshare_network | pf::signal_parent, 4096*10);
        }, pf::fork, 4096*10);
        //child.init_user_namespace();
        pipe.in().close();
        pipe.out().write("x", 1);
        ret = child.wait().exit_code();
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        app.terminate();
    }
    return ret;
}

int main(int argc, char* argv[]) {
    bool restart = false;
    int ret = nested_main(argc, argv, restart);
    if (restart && !std::getenv("DTEST_NO_RESTART")) {
        ::setenv("DTEST_NO_RESTART", "", 1);
        sys::log_message("dtest", "restart after power failure");
        sys::this_process::execute(argv);
    }
    return ret;
}
