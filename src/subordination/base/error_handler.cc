#include "error_handler.hh"

#include <unistd.h>

#include <exception>
#include <iostream>

#include <unistdx/base/log_message>
#include <unistdx/ipc/process>
#include <unistdx/ipc/signal>
#include <unistdx/util/backtrace>

#include <subordination/base/error.hh>
#include <subordination/base/thread_name.hh>
#include <subordination/kernel/kernel_error.hh>

void
sbn::print_backtrace(int sig) noexcept {
    sys::log_message(
        "error_handler",
        "caught _",
        sys::signal(sig)
    );
    sys::backtrace(STDERR_FILENO);
    std::exit(sig);
}

void
sbn::print_error() noexcept {
    using namespace sys;
    if (std::exception_ptr ptr = std::current_exception()) {
        try {
            std::rethrow_exception(ptr);
        } catch (const kernel_error& err) {
            sys::log_message(
                "error_handler",
                "error=_, thread=_-_, process=_",
                err,
                this_thread::name,
                this_thread::number,
                this_process::id()
            );
        } catch (const error& err) {
            sys::log_message(
                "error_handler",
                "error=_, thread=_-_, process=_",
                err,
                this_thread::name,
                this_thread::number,
                this_process::id()
            );
        } catch (const sys::bad_call& err) {
            sys::log_message(
                "error_handler",
                "error=_, thread=_-_, process=_",
                err,
                this_thread::name,
                this_thread::number,
                this_process::id()
            );
        } catch (const std::exception& err) {
            sys::log_message(
                "error_handler",
                "error=_, thread=_-_, process=_",
                err.what(),
                this_thread::name,
                this_thread::number,
                this_process::id()
            );
        } catch (...) {
            sys::log_message(
                "error_handler",
                "error=_, thread=_-_, process=_",
                "<unknown>",
                this_thread::name,
                this_thread::number,
                this_process::id()
            );
        }
    } else {
        sys::log_message(
            "error_handler",
            "error=_, thread=_-_, process=_",
            "<none>",
            this_thread::name,
            this_thread::number,
            this_process::id()
        );
    }
    sys::backtrace(STDERR_FILENO);
    std::exit(1);
}

void
sbn::install_error_handler() {
    using namespace sys::this_process;
    std::set_terminate(print_error);
    std::set_unexpected(print_error);
    ignore_signal(sys::signal::broken_pipe);
    bind_signal(sys::signal::segmentation_fault, print_backtrace);
    bind_signal(sys::signal::abort, print_backtrace);
}

