#ifndef FACTORY_BITS_TERMINATE_HANDLER_HH
#define FACTORY_BITS_TERMINATE_HANDLER_HH

#include <factory/error.hh>
#include <sys/bits/check.hh>

namespace factory {

	struct Thread_id {
		friend std::ostream&
		operator<<(std::ostream& out, const Thread_id& rhs) {
			return out << "(thread id=" << std::this_thread::get_id() << ')';
		}
	};

	// TODO 2016-04-30 do we need this, or default behaviour is OK?
	struct Terminate_guard {

		Terminate_guard() noexcept {
			std::set_terminate(Terminate_guard::error_printing_handler);
			init_signal_handlers();
		}

	private:

		enum struct Exit_code: int {
			Success = 0,
			Failure = -1
		};

		static void
		error_printing_handler() noexcept {
			static volatile bool called = false;
			if (called) { return; }
			called = true;
			if (std::exception_ptr ptr = std::current_exception()) {
				try {
					std::rethrow_exception(ptr);
				} catch (Error& err) {
					std::cerr << err << Thread_id() << std::endl;
				} catch (sys::bits::bad_call& err) {
					std::cerr << err << Thread_id() << std::endl;
				} catch (std::exception& err) {
					std::cerr << err << Thread_id() << std::endl;
				} catch (...) {
					std::cerr << "unknown exception caught" << Thread_id() << std::endl;
				}
			} else {
				std::cerr << "terminate called without an active exception" << Thread_id() << std::endl;
			}
		}

		void
		init_signal_handlers() noexcept {
			//sys::this_process::bind_signal(sys::signal::terminate, normal_shutdown);
			//sys::this_process::bind_signal(sys::signal::keyboard_interrupt, normal_shutdown);
			sys::this_process::ignore_signal(sys::signal::broken_pipe);
		}

	};

}
#endif // FACTORY_BITS_TERMINATE_HANDLER_HH
