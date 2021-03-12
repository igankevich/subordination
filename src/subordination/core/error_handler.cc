#include <unistdx/ipc/signal>
#include <unistdx/system/error>

#include <subordination/core/error_handler.hh>

void sbn::install_error_handler() {
    using namespace sys::this_process;
    using s = sys::signal;
    std::set_terminate(sys::backtrace_on_terminate);
    std::set_unexpected(sys::backtrace_on_terminate);
    ignore_signal(s::broken_pipe);
    bind_signal(s::segmentation_fault, sys::backtrace_on_signal);
    bind_signal(s::abort, sys::backtrace_on_signal);
}
