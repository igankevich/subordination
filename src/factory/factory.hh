#ifndef FACTORY_FACTORY_HH
#define FACTORY_FACTORY_HH

// autoconf defines
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

// Patching stdc++ for Valgrind.
#ifdef HAVE_VALGRIND
#include <valgrind/drd.h>
#include <valgrind/helgrind.h>
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A) ANNOTATE_HAPPENS_BEFORE(A)
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A)  ANNOTATE_HAPPENS_AFTER(A)
#endif

#if __cplusplus < 201103L
	#error Factory requires C++11 compiler.
#endif

#include <sys/process.hh>
#include <sys/log.hh>
#include <sys/packetstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

namespace factory {

	namespace components {

		template<class Config>
		struct Basic_factory:
		public Server<Config>,
//		private sys::Disable_sync_with_stdio,
		private sys::Auto_check_endiannes,
//		private sys::Auto_filter_bad_chars_on_cout_and_cerr,
//		private sys::Auto_open_standard_file_descriptors,
		private Auto_set_terminate_handler<Basic_factory<Config>>,
		public sys::Install_syslog
		{

			typedef Server<Config> base_server;
			using typename base_server::kernel_type;
			typedef typename kernel_type::app_type app_type;
			typedef typename Config::local_server local_server_type;
			typedef typename Config::remote_server remote_server_type;
			typedef typename Config::timer_server timer_server_type;
			typedef typename kernel_type::id_type id_type;

			Basic_factory(Global_thread_context& context):
			Auto_set_terminate_handler<Basic_factory>(this),
			Install_syslog(std::clog),
			_globalcon(&context)
			{
				Install_syslog::tee(true);
				this->setfactory(this);
				init_parents();
			}

			virtual ~Basic_factory() {}

			void
			start() override {
				base_server::start();
				_local_server.start();
				_remote_server.start();
				_timer_server.start();
			}

			void
			stop() override {
				base_server::stop();
				_local_server.stop();
				_remote_server.stop();
				_timer_server.stop();
			}

			void wait() override {
				_local_server.wait();
				_remote_server.wait();
				_timer_server.wait();
			}

			void
			shutdown() override {
				base_server::shutdown();
				_local_server.shutdown();
				_remote_server.shutdown();
				_timer_server.shutdown();
				stop();
			}

			void send(kernel_type* k) override { this->_local_server.send(k); }

			local_server_type* local_server() { return &_local_server; }
			remote_server_type* remote_server() { return &_remote_server; }
			timer_server_type* timer_server() { return &_timer_server; }

			void
			run(int argc, char* argv[]) {
				try {
					do_config(argc, argv);
					this->start();
					do_run(argc, argv);
					this->wait();
				} catch (std::exception& e) {
					std::cerr << e.what() << std::endl;
					set_exit_code(EXIT_FAILURE);
				}
			}

			virtual void
			do_run(int argc, char* argv[]) {}

			virtual void
			do_config(int argc, char* argv[]) {}

			int
			exit_code() const noexcept {
				return _exitcode;
			}

			void
			set_exit_code(int rhs) noexcept {
				_exitcode = rhs;
			}

			Instances<kernel_type>& instances() {
				return _instances;
			}

			Types<Type<kernel_type>>& types() {
				return _types;
			}

			app_type
			app() const noexcept {
				return Application::ROOT;
			}

			Global_thread_context*
			global_context() noexcept override {
				return _globalcon;
			}

		private:

			void init_parents() {
				this->_local_server.setparent(this);
				this->_remote_server.setparent(this);
				this->_timer_server.setparent(this);
			}

			local_server_type _local_server;
			remote_server_type _remote_server;
			timer_server_type _timer_server;
			int _exitcode = 0;
			Instances<kernel_type> _instances;
			Types<Type<kernel_type>> _types;
			Global_thread_context* _globalcon = nullptr;
		};

		template<class Main, class Config>
		struct App: public Basic_factory<Config> {

			typedef Basic_factory<Config> base_type;

			App(Global_thread_context& ctx):
			Basic_factory<Config>(ctx)
			{}

			void
			do_config(int argc, char* argv[]) override {
				_main = new Main(*this, argc, argv);
			}

			void
			do_run(int argc, char* argv[]) override {
				this->local_server()->send(_main);
			}

		private:
			Main* _main;
		};

	}

}

namespace factory {

	template<class Main, class Config>
	int
	factory_main(int argc, char* argv[]) noexcept {
		factory::components::Global_thread_context ctx;
		factory::components::App<Main,Config> app(ctx);
		app.run(argc, argv);
		return app.exit_code();
	}

}

#endif // FACTORY_FACTORY_HH
