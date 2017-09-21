#include "error_handler.hh"

#include <exception>
#include <bscheduler/base/error.hh>
#include <bscheduler/kernel/kernel_error.hh>
#include <iostream>
#include <unistd.h>
#include <unistdx/base/log_message>
#include <unistdx/ipc/signal>
#include <unistdx/util/backtrace>

#include <bscheduler/base/thread_name.hh>

void
bsc::print_backtrace(int sig) noexcept {
	sys::log_message(
		std::cerr,
		"error_handler",
		"caught _",
		sys::signal(sig)
	);
	sys::backtrace(STDERR_FILENO);
	std::exit(sig);
}

void
bsc::print_error() noexcept {
	if (std::exception_ptr ptr = std::current_exception()) {
		try {
			std::rethrow_exception(ptr);
		} catch (const kernel_error& err) {
			sys::log_message(
				std::cerr,
				"error_handler",
				"error=_, thread=_-_",
				err,
				this_thread::name,
				this_thread::number
			);
		} catch (const error& err) {
			sys::log_message(
				std::cerr,
				"error_handler",
				"error=_, thread=_-_",
				err,
				this_thread::name,
				this_thread::number
			);
		} catch (const sys::bad_call& err) {
			sys::log_message(
				std::cerr,
				"error_handler",
				"error=_, thread=_-_",
				err,
				this_thread::name,
				this_thread::number
			);
		} catch (const std::exception& err) {
			sys::log_message(
				std::cerr,
				"error_handler",
				"error=_, thread=_-_",
				err.what(),
				this_thread::name,
				this_thread::number
			);
		} catch (...) {
			sys::log_message(
				std::cerr,
				"error_handler",
				"error=_, thread=_-_",
				"<unknown>",
				this_thread::name,
				this_thread::number
			);
		}
	} else {
		sys::log_message(
			std::cerr,
			"error_handler",
			"error=_, thread=_-_",
			"<none>",
			this_thread::name,
			this_thread::number
		);
	}
	sys::backtrace(STDERR_FILENO);
	std::exit(1);
}

void
bsc::install_error_handler() {
	using namespace sys::this_process;
	std::set_terminate(print_error);
	std::set_unexpected(print_error);
	ignore_signal(sys::signal::broken_pipe);
	bind_signal(sys::signal::segmentation_fault, print_backtrace);
}

