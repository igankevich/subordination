#ifndef FACTORY_BITS_TERMINATE_HANDLER_HH
#define FACTORY_BITS_TERMINATE_HANDLER_HH

#include <factory/error.hh>
#include <stdx/log.hh>
#include <sysx/bits/check.hh>

namespace factory {

	namespace components {

		template<class Server>
		struct Auto_set_terminate_handler {

			typedef stdx::log<Auto_set_terminate_handler> this_log;
			typedef Server server_type;

			explicit
			Auto_set_terminate_handler(server_type* root) noexcept {
				_root = root;
				std::set_terminate(Auto_set_terminate_handler::error_printing_handler);
				init_signal_handlers();
			}

		private:

			static void
			error_printing_handler() noexcept {
				static volatile bool called = false;
				if (called) { return; }
				called = true;
				std::exception_ptr ptr = std::current_exception();
				if (ptr) {
					try {
						std::rethrow_exception(ptr);
					} catch (sysx::bits::os_error& err) {
						this_log() << err << std::endl;
					} catch (Error& err) {
						this_log() << Error_message(err, __FILE__, __LINE__, __func__) << std::endl;
					} catch (std::exception& err) {
						this_log() << String_message(err, __FILE__, __LINE__, __func__) << std::endl;
					} catch (...) {
						this_log() << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__) << std::endl;
					}
				} else {
					this_log() << String_message("terminate called without an active exception",
						__FILE__, __LINE__, __func__) << std::endl;
				}
				stop_root_server();
			}

			void
			init_signal_handlers() noexcept {
				sysx::this_process::bind_signal(SIGTERM, emergency_shutdown);
				sysx::this_process::bind_signal(SIGINT, emergency_shutdown);
				sysx::this_process::bind_signal(SIGPIPE, SIG_IGN);
			}

			static void
			emergency_shutdown(int) noexcept {
				stop_root_server();
			}

			static void
			stop_root_server() {
				if (_root) {
					_root->stop();
				}
			}

		private:
			static server_type* _root;
		};

		template<class Server>
		typename Auto_set_terminate_handler<Server>::server_type*
		Auto_set_terminate_handler<Server>::_root = nullptr;

	}
	
}
#endif // FACTORY_BITS_TERMINATE_HANDLER_HH
