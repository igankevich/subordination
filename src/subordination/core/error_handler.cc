#include <subordination/core/error.hh>

#include <unistd.h>

#include <exception>
#include <iostream>

#include <unistdx/base/log_message>
#include <unistdx/ipc/process>
#include <unistdx/ipc/signal>
#include <unistdx/util/backtrace>

#include <subordination/core/error.hh>
#include <subordination/core/error_handler.hh>

template <class ... Args>
inline void
message(const Args& ... args) {
    sys::log_message("error", args ...);
}

void sbn::print_backtrace(int sig) noexcept {
    char name[16] {'\0'};
    #if defined(UNISTDX_HAVE_PRCTL)
    ::prctl(PR_GET_NAME, name);
    #endif
    message("process \"_\" caught _", name, sys::signal(sig));
    sys::backtrace(STDERR_FILENO);
    std::exit(sig);
}

void sbn::print_error() noexcept {
    using namespace sys;
    char name[16] {'\0'};
    #if defined(UNISTDX_HAVE_PRCTL)
    ::prctl(PR_GET_NAME, name);
    #endif
    if (std::exception_ptr ptr = std::current_exception()) {
        try {
            std::rethrow_exception(ptr);
        } catch (const error& err) {
            message("error=_, process=_", err, name);
        } catch (const std::exception& err) {
            message("error=_, process=_", err.what(), name);
        } catch (...) {
            message("error=_, process=_", "<unknown>", name);
        }
        sys::backtrace(STDERR_FILENO);
    }
    std::exit(1);
}

void
sbn::install_error_handler() {
    using namespace sys::this_process;
    std::set_terminate(print_error);
    std::set_unexpected(print_error);
    ignore_signal(sys::signal::broken_pipe);
    bind_signal(sys::signal::segmentation_fault, print_backtrace);
}
