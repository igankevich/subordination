#ifndef SUBORDINATION_CORE_ERROR_HANDLER_HH
#define SUBORDINATION_CORE_ERROR_HANDLER_HH

namespace sbn {

    [[noreturn]] void
    print_backtrace(int sig) noexcept;

    [[noreturn]] void
    print_error() noexcept;

    void
    install_error_handler();

}
#endif // vim:filetype=cpp
