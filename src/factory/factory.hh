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

#include <sysx/security.hh>
#include <sysx/process.hh>
#include <sysx/log.hh>
#include <sysx/packetstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

namespace factory {

	namespace components {

		template<class Config>
		struct Basic_factory:
		public Managed_set<Server<Config>>,
//		private sysx::Disable_sync_with_stdio,
		private sysx::Auto_check_endiannes,
		private sysx::Auto_filter_bad_chars_on_cout_and_cerr,
		private sysx::Auto_open_standard_file_descriptors,
		private Auto_set_terminate_handler<Basic_factory<Config>>,
		public sysx::Install_syslog
		{

			typedef Managed_set<Server<Config>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::for_each_func_type;
			typedef typename kernel_type::app_type app_type;
			typedef typename Config::local_server Local_server;
			typedef typename Config::remote_server Remote_server;
			typedef typename Config::external_server External_server;
			typedef typename Config::timer_server Timer_server;
			typedef typename kernel_type::id_type id_type;

			Basic_factory(Global_thread_context& context):
			Auto_set_terminate_handler<Basic_factory>(this),
			Install_syslog(std::clog),
			_local_server(),
			_remote_server(),
			_ext_server(),
			_timer_server()
			{
				Install_syslog::tee(true);
				this->setfactory(this);
				init_parents();
				init_names();
				init_context(&context);
			}

			virtual ~Basic_factory() {}

			Category
			category() const noexcept override {
				return Category{
					"factory",
					[] () { return nullptr; }
				};
			}

			void
			start() override {
				base_server::start();
				_local_server.start();
				_remote_server.start();
				_ext_server.start();
				_timer_server.start();
			}

			void
			stop() override {
				base_server::stop();
				_local_server.stop();
				_remote_server.stop();
				_ext_server.stop();
				_timer_server.stop();
			}

			void wait() override {
				_local_server.wait();
				_remote_server.wait();
				_ext_server.wait();
				_timer_server.wait();
			}

			void
			shutdown() override {
				base_server::shutdown();
				_local_server.shutdown();
				_remote_server.shutdown();
				_ext_server.shutdown();
				_timer_server.shutdown();
				stop();
			}

			void send(kernel_type* k) override { this->_local_server.send(k); }

			Local_server* local_server() { return &_local_server; }
			Remote_server* remote_server() { return &_remote_server; }
			External_server* ext_server() { return &_ext_server; }
			Timer_server* timer_server() { return &_timer_server; }

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

			void
			write_recursively(std::ostream& out) const override {
				base_server::write_recursively(out);
				this->write_foreign(_types, "types", out);
			}

			app_type
			app() const noexcept {
				return Application::ROOT;
			}

		private:

			void init_parents() {
				this->_local_server.setparent(this);
				this->_remote_server.setparent(this);
				this->_ext_server.setparent(this);
				this->_timer_server.setparent(this);
			}

			void init_names() {
				this->setname("root");
				_local_server.setname("local");
				_remote_server.setname("remote");
				_ext_server.setname("ext");
				_timer_server.setname("timer");
			}

			void init_context(Global_thread_context* context) {
				this->_local_server.set_global_context(context);
				this->_remote_server.set_global_context(context);
				this->_ext_server.set_global_context(context);
				this->_timer_server.set_global_context(context);
			}

			Local_server _local_server;
			Remote_server _remote_server;
			External_server _ext_server;
			Timer_server _timer_server;
			int _exitcode = 0;
			Instances<kernel_type> _instances;
			Types<Type<kernel_type>> _types;
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
