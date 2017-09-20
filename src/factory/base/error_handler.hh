#ifndef FACTORY_BASE_ERROR_HANDLER_HH
#define FACTORY_BASE_ERROR_HANDLER_HH

namespace asc {

	[[noreturn]] void
	print_backtrace(int sig) noexcept;

	[[noreturn]] void
	print_error() noexcept;

	void
	install_error_handler();

}
#endif // vim:filetype=cpp
