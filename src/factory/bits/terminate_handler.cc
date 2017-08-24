#include "terminate_handler.hh"

#include <factory/error.hh>
#include <iostream>
#include <thread>
#include <exception>
#include <unistd.h>
#include <unistdx/base/log_message>
#include <unistdx/ipc/signal>
#include <unistdx/util/backtrace>

namespace {

	volatile bool called = false;

}

void
factory::print_backtrace(int sig) noexcept {
	sys::log_message(
		std::cerr,
		"terminate_handler",
		"caught _",
		sys::signal(sig)
	);
	sys::backtrace(STDERR_FILENO);
	std::exit(sig);
}

void
factory::print_error() noexcept {
	if (called) { return; }
	called = true;
	const auto tid = std::this_thread::get_id();
	if (std::exception_ptr ptr = std::current_exception()) {
		try {
			std::rethrow_exception(ptr);
		} catch (const Error& err) {
			sys::log_message(
				std::cerr,
				"terminate_handler",
				"terminated with error _, thread=_",
				err,
				tid
			);
		} catch (const sys::bad_call& err) {
			sys::log_message(
				std::cerr,
				"terminate_handler",
				"terminated with error _, thread=_",
				err,
				tid
			);
		} catch (const std::exception& err) {
			sys::log_message(
				std::cerr,
				"terminate_handler",
				"terminated with error _, thread=_",
				err.what(),
				tid
			);
		} catch (...) {
			sys::log_message(
				std::cerr,
				"terminate_handler",
				"terminated with error _, thread=_",
				"<unknown>",
				tid
			);
		}
	} else {
		sys::log_message(
			std::cerr,
			"terminate_handler",
			"terminated with error _, thread=_",
			"<no error>",
			tid
		);
	}
	sys::backtrace(STDERR_FILENO);
	std::exit(1);
}

factory::Terminate_guard::Terminate_guard() noexcept {
	using namespace sys::this_process;
	std::set_terminate(print_error);
	std::set_unexpected(print_error);
	ignore_signal(sys::signal::broken_pipe);
	bind_signal(sys::signal::segmentation_fault, print_backtrace);
}

