#include <iostream>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/io/pipe>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/process>
#include <unistdx/ipc/signal>

#include <dtest/application.hh>

using s = sys::signal;

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("dtest", args...);
}

int nested_main(int argc, char* argv[], bool& will_restart) {
    int ret = EXIT_FAILURE;
    dts::application app;
    try {
        app.init(argc, argv);
        will_restart = app.will_restart();
        ret = run(app);
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
