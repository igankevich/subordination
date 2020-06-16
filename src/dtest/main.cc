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
sys::pid_type child_process_id = 0;

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("dtest", args...);
}

void parent_signal_handlers() {
    using namespace sys::this_process;
    auto on_terminate = sys::signal_action([](int sig) {
        if (child_process_id) {
            try {
                sys::send(s::terminate, child_process_id);
            } catch (const std::exception& err) {
                const char* msg = "error in parent signal handler";
                ::write(2, msg, std::string::traits_type::length(msg));
                ::write(2, err.what(), std::string::traits_type::length(err.what()));
            }
        }
    });
    ignore_signal(s::broken_pipe);
    bind_signal(s::keyboard_interrupt, on_terminate);
    bind_signal(s::terminate, on_terminate);
}

void child_signal_handlers() {
    using namespace sys::this_process;
    auto on_terminate = sys::signal_action([](int sig) {
        const auto is_alarm = sys::signal(sig) == sys::signal::alarm;
        if (aptr) {
            try {
                aptr->send(is_alarm ? s::kill : s::terminate);
            } catch (const std::exception& err) {
                ::write(2, err.what(), std::string::traits_type::length(err.what()));
            }
        }
        if (!is_alarm) { ::alarm(3); }
    });
    ignore_signal(s::broken_pipe);
    bind_signal(s::terminate, on_terminate);
    bind_signal(s::keyboard_interrupt, on_terminate);
    bind_signal(s::alarm, on_terminate);
}

int nested_main(int argc, char* argv[], bool& will_restart) {
    int ret = EXIT_FAILURE;
    dts::application app;
    aptr = &app;
    try {
        parent_signal_handlers();
        app.init(argc, argv);
        will_restart = app.will_restart();
        sys::pipe pipe;
        pipe.in().unsetf(sys::open_flag::non_blocking);
        pipe.out().unsetf(sys::open_flag::non_blocking);
        using pf = sys::process_flag;
        sys::process child([&pipe,&app] () {
            try {
                child_signal_handlers();
                pipe.out().close();
                char ch;
                pipe.in().read(&ch, 1);
                //using f = sys::unshare_flag;
                //sys::this_process::unshare(f::users | f::network);
                app.run();
                return app.wait();
            } catch (const std::exception& err) {
                std::cerr << err.what() << std::endl;
                return 1;
            }
        },
        pf::unshare_users | pf::unshare_network | pf::unshare_hostname | pf::signal_parent,
        4096*10);
        child_process_id = child.id();
        //}, pf::fork, 4096*10);
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

int main(int argc, char* argv[]) {
    bool restart = false;
    int ret = nested_main(argc, argv, restart);
    if (restart && !std::getenv("DTEST_NO_RESTART")) {
        ::setenv("DTEST_NO_RESTART", "", 1);
        log("restart after power failure");
        sys::this_process::execute(argv);
    }
    log("terminated");
    return ret;
}
