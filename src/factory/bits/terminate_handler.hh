#ifndef FACTORY_BITS_TERMINATE_HANDLER_HH
#define FACTORY_BITS_TERMINATE_HANDLER_HH

#include <factory/error.hh>
#include <stdx/log.hh>
#include <sys/bits/check.hh>
#include <sys/log_color.hh>

namespace factory {

	namespace components {

		template<class T>
		struct Red {

			explicit
			Red(const T& rhs) noexcept:
			_object(rhs)
			{}

			friend std::ostream&
			operator<<(std::ostream& out, const Red& rhs) {
				return
				out << sys::log_color::FG_LIGHT_RED
					<< rhs._object
					<< sys::log_color::RESET;
			}

		private:

			const T& _object;

		};

		template<class T>
		Red<T>
		make_red(const T& rhs) noexcept {
			return Red<T>(rhs);
		}

		struct Thread_id {
			friend std::ostream&
			operator<<(std::ostream& out, const Thread_id& rhs) {
				return out << "(thread id=" << std::this_thread::get_id() << ')';
			}
		};

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

			enum struct exit_code: int {
				success = 0,
				failure = -1
			};

			static void
			error_printing_handler() noexcept {
				static volatile bool called = false;
				if (called) { return; }
				called = true;
				std::exception_ptr ptr = std::current_exception();
				if (ptr) {
					try {
						std::rethrow_exception(ptr);
					} catch (Error& err) {
						this_log() << err << Thread_id() << std::endl;
					} catch (std::exception& err) {
						this_log() << make_red(err) << Thread_id() << std::endl;
					} catch (...) {
						this_log() << make_red("unknown exception caught") << Thread_id() << std::endl;
					}
				} else {
					this_log() << make_red("terminate called without an active exception") << Thread_id() << std::endl;
				}
				stop_root_server(exit_code::failure);
			}

			void
			init_signal_handlers() noexcept {
				sys::this_process::bind_signal(sys::signal::terminate, normal_shutdown);
				sys::this_process::bind_signal(sys::signal::keyboard_interrupt, normal_shutdown);
				sys::this_process::ignore_signal(sys::signal::broken_pipe);
			}

			static void
			normal_shutdown(int) noexcept {
				stop_root_server(exit_code::success);
			}

			static void
			emergency_shutdown(int sig) noexcept {
				stop_root_server(exit_code(sig));
			}

			static void
			stop_root_server(exit_code retval) {
				if (_root) {
					_root->set_exit_code(int(retval));
					_root->shutdown();
				}
			}

			static server_type* _root;
		};

		template<class Server>
		typename Auto_set_terminate_handler<Server>::server_type*
		Auto_set_terminate_handler<Server>::_root = nullptr;

	}

}
#endif // FACTORY_BITS_TERMINATE_HANDLER_HH
