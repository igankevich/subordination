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
#include <sysx/cpu_bind.hh>
#include <sysx/packstream.hh>

#include <factory/bits/terminate_handler.hh>
#include <factory/kernel.hh>

namespace factory {

	namespace components {

		template<class Server>
		struct Daemon {

			explicit
			Daemon(Server& rhs) noexcept:
			_server(rhs)
			{
				std::cerr << "Starting daemon" << std::endl;
				_server.start();
				_server.wait();
			}

			~Daemon() {
				if (!_server.stopped()) {
					std::cerr << "Stopping daemon" << std::endl;
					_server.stop();
					_server.wait();
				} else {
					std::cerr << "Daemon is stopped" << std::endl;
				}
			}

		private:
			Server& _server;
		};

		template<class Config>
		struct Shutdown: public Config::kernel {

			typedef stdx::log<Shutdown> this_log;
			typedef typename Config::server server_type;
			typedef typename Config::kernel Kernel;

			void act(server_type& this_server) override {
				this_log() << "broadcasting shutdown message" << std::endl;
				delete this;
				this_server.factory()->stop();
			}

			const Type<Kernel>
			type() const noexcept override {
				return static_type();
			}

			static const Type<Kernel>
			static_type() noexcept {
				return Type<Kernel>{
					SHUTDOWN_ID,
					"Shutdown",
					[] (sysx::packstream& in) {
						Shutdown* k = new Shutdown;
						k->read(in);
						return k;
					}
				};
			}

		};

		template<class Config>
		struct Basic_factory: public Managed_set<Server<Config>>,
			private sysx::Auto_check_endiannes,
			private sysx::Auto_filter_bad_chars_on_cout_and_cerr,
			private sysx::Auto_open_standard_file_descriptors,
			private Auto_set_terminate_handler<Server<Config>>,
			private sysx::Install_syslog
		{

			typedef Managed_set<Server<Config>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::for_each_func_type;
			typedef typename Config::local_server Local_server;
			typedef typename Config::remote_server Remote_server;
			typedef typename Config::external_server External_server;
			typedef typename Config::timer_server Timer_server;
			typedef typename Config::app_server App_server;
			typedef typename Config::principal_server Principal_server;
			typedef Shutdown<Config> shutdown_type;

			enum struct Role {
				Principal,
				Subordinate
			};

			Basic_factory(Global_thread_context& context):
				Auto_set_terminate_handler<Server<Config>>(this),
				Install_syslog(std::clog),
				_local_server(),
				_remote_server(),
				_ext_server(),
				_timer_server(),
				_app_server(),
				_principal_server()
			{
				this->setfactory(this);
				init_parents();
				init_names();
				init_context(&context);
				types().register_type(shutdown_type::static_type());
				std::cout << std::endl;
				this->dump_hierarchy(std::cout);
				std::cout << std::endl;
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
				_app_server.start();
				if (_role == Role::Subordinate) {
					_principal_server.start();
				}
			}

			void
			stop() override {
				base_server::stop();
				_remote_server.send(new shutdown_type);
				_app_server.send(new shutdown_type);
				if (_role == Role::Subordinate) {
					_principal_server.send(new shutdown_type);
				}
				_local_server.stop();
				_remote_server.stop();
				_ext_server.stop();
				_timer_server.stop();
				_app_server.stop();
				if (_role == Role::Subordinate) {
					_principal_server.stop();
				}
//				_remote_server.send(new Shutdown);
//				_ext_server.send(new Shutdown);
//				_app_server.send(new Shutdown);
//				if (_role == Role::Subordinate) {
//					_principal_server.send(new Shutdown);
//				}
//				Shutdown* s = new Shutdown(true);
//				s->after(std::chrono::milliseconds(500));
//				_timer_server.send(s);
			}

			void wait() override {
				_local_server.wait();
				_remote_server.wait();
				_ext_server.wait();
				_timer_server.wait();
				_app_server.wait();
				if (_role == Role::Subordinate) {
					_principal_server.wait();
				}
			}

			void send(kernel_type* k) override { this->_local_server.send(k); }

			Local_server* local_server() { return &_local_server; }
			Remote_server* remote_server() { return &_remote_server; }
			External_server* ext_server() { return &_ext_server; }
			Timer_server* timer_server() { return &_timer_server; }
			App_server* app_server() { return &_app_server; }
			Principal_server* principal_server() { return &_principal_server; }

			sysx::endpoint addr() const { return _remote_server.server_addr(); }

			void setrole(Role rhs) { this->_role = rhs; }

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

			template<class Kernel, class ... Args>
			Kernel*
			new_kernel(Args&& ... args) {
				Kernel* k = new Kernel(std::forward<Args>(args)...);
				if (std::is_convertible<Kernel, Identifiable_tag>::value) {
					k->id(factory_generate_id());
					instances().register_instance(k);
				}
				return k;
			}

			Id
			factory_generate_id() {
				return _counter++;
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

		private:

			Id
			factory_start_id() noexcept {

				struct factory_start_id_t{};
				typedef stdx::log<factory_start_id_t> this_log;

				constexpr static const Id
				DEFAULT_START_ID = 1000;

				Id i = sysx::this_process::getenv("START_ID", DEFAULT_START_ID);
				if (i == ROOT_ID) {
					i = DEFAULT_START_ID;
					this_log() << "Bad START_ID value: " << ROOT_ID << std::endl;
				}
				this_log() << "START_ID = " << i << std::endl;
				return i;
			}

			void init_parents() {
				this->_local_server.setparent(this);
				this->_remote_server.setparent(this);
				this->_ext_server.setparent(this);
				this->_timer_server.setparent(this);
				this->_app_server.setparent(this);
				this->_principal_server.setparent(this);
			}

			void init_names() {
				this->setname("root");
				_local_server.setname("local");
				_remote_server.setname("remote");
				_ext_server.setname("ext");
				_timer_server.setname("timer");
				_app_server.setname("app");
				_principal_server.setname("princ");
			}

			void init_context(Global_thread_context* context) {
				this->_local_server.set_global_context(context);
				this->_remote_server.set_global_context(context);
				this->_ext_server.set_global_context(context);
				this->_timer_server.set_global_context(context);
				this->_app_server.set_global_context(context);
				this->_principal_server.set_global_context(context);
			}

			Local_server _local_server;
			Remote_server _remote_server;
			External_server _ext_server;
			Timer_server _timer_server;
			App_server _app_server;
			Principal_server _principal_server;
			Role _role = Role::Principal;
			int _exitcode = 0;
			std::atomic<Id> _counter{factory_start_id()};
			Instances<kernel_type> _instances;
			Types<Type<kernel_type>> _types;
		};

		template<class Main, class Config>
		struct App: public Basic_factory<Config> {

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
