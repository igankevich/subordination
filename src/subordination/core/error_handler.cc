#include <unistd.h>

#include <exception>
#include <iostream>

#include <unistdx/base/log_message>
#include <unistdx/io/fildes>
#include <unistdx/ipc/process>
#include <unistdx/ipc/signal>
#include <unistdx/system/error>

#include <subordination/core/error.hh>
#include <subordination/core/error_handler.hh>

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("error", args ...);
}

void sbn::print_backtrace(int sig) noexcept {
    #if defined(SBN_DEBUG)
    sys::backtrace_on_signal(STDERR_FILENO);
    #else
    sys::backtrace(STDERR_FILENO);
    #endif
    std::exit(sig);
}

void sbn::print_error() noexcept {
    if (std::exception_ptr ptr = std::current_exception()) {
        const char* msg = "unexpected exception";
        try {
            std::rethrow_exception(ptr);
        } catch (const std::exception& err) {
            msg = err.what();
        } catch (...) {
        }
        message("_", sys::error(msg).what());
    }
    std::exit(1);
}

void
sbn::install_error_handler() {
    using namespace sys::this_process;
    using s = sys::signal;
    std::set_terminate(sys::dump_core);
    std::set_unexpected(sys::dump_core);
    ignore_signal(s::broken_pipe);
    bind_signal(s::segmentation_fault, print_backtrace);
    bind_signal(s::abort, print_backtrace);
}
