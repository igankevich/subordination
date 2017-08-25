#ifndef FACTORY_BITS_TERMINATE_HANDLER_HH
#define FACTORY_BITS_TERMINATE_HANDLER_HH

namespace factory {

	[[noreturn]] void
	print_backtrace(int sig) noexcept;

	[[noreturn]] void
	print_error() noexcept;

	void
	install_error_handler();

}
#endif // FACTORY_BITS_TERMINATE_HANDLER_HH
