#ifndef FACTORY_BITS_TERMINATE_HANDLER_HH
#define FACTORY_BITS_TERMINATE_HANDLER_HH

namespace factory {

	[[noreturn]] void
	print_backtrace(int sig) noexcept;

	void
	print_error() noexcept;

	struct Terminate_guard {

		Terminate_guard() noexcept;
		~Terminate_guard() noexcept = default;

		Terminate_guard(const Terminate_guard&) = delete;
		Terminate_guard(Terminate_guard&&) = delete;
		Terminate_guard&
		operator=(const Terminate_guard&) = delete;

	};

}
#endif // FACTORY_BITS_TERMINATE_HANDLER_HH
