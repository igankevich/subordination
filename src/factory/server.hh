#include "bits/security.hh"
#include "bits/cpu_bind.hh"
#include "server/basic_server.hh"
#include "server/cpu_server.hh"
#include "server/timer_server.hh"
#include "server/nic_server.hh"

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

		template<class T>
		struct Shutdown: public Type_init<Shutdown<T>, T> {

			typedef stdx::log<Shutdown> this_log;

			void act(Managed_object<Server<Principal>>& this_server) override {
				this_log() << "broadcasting shutdown message" << std::endl;
				delete this;
				this_server.root()->stop();
			}

			void read_impl(packstream&) {}
			void write_impl(packstream&) {}

			static void
			init_type(Type<T>* t) {
				t->id(SHUTDOWN_ID);
				t->name("Shutdown");
			}

		};

		template<
			class Kernel,
			class Local_server,
			class Remote_server,
			class External_server,
			class Timer_server,
			class App_server,
			class Principal_server
		>
		struct Basic_factory: public Managed_set<Server<Kernel>>,
			private Auto_check_endiannes,
			private Auto_filter_bad_chars_on_cout_and_cerr,
			private Auto_open_standard_file_descriptors,
			private Auto_set_terminate_handler<Server<Kernel>>
		{

			typedef Kernel kernel_type;
			typedef Server<kernel_type> base_server;
			typedef Shutdown<kernel_type> shutdown_type;

			enum struct Role {
				Principal,
				Subordinate
			};

			Basic_factory():
				Auto_set_terminate_handler<base_server>(this),
				_local_server(),
				_remote_server(),
				_ext_server(),
				_timer_server(),
				_app_server(),
				_principal_server()
			{
				init_parents();
				init_names();
//				register_factory(this);
				std::cout << std::endl;
				this->dump_hierarchy(std::cout);
				std::cout << std::endl;
			}

			virtual ~Basic_factory() {}

			Category
			category() const noexcept override {
				return Category{
					"factory",
					[] () { return new Basic_factory; }
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

			unix::endpoint addr() const { return _remote_server.server_addr(); }

			void setrole(Role rhs) { this->_role = rhs; }

		private:

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

			Local_server _local_server;
			Remote_server _remote_server;
			External_server _ext_server;
			Timer_server _timer_server;
			App_server _app_server;
			Principal_server _principal_server;
			Role _role = Role::Principal;
		};

	}

}

