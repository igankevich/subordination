#include <signal.h>
#include <strings.h>
#include <fenv.h>

namespace factory {

	struct Shutdown: public Mobile<Shutdown> {
		void act() {
			Logger(Level::COMPONENT) << "broadcasting shutdown message" << std::endl;
			components::factory_stop();
		}
		void react() {}
		void write_impl(Foreign_stream&) {}
		void read_impl(Foreign_stream&) {}
		bool broadcast() const { return true; }
		static void init_type(Type* t) { t->id(123); }
	};


	void emergency_shutdown(int signum);
	void sigpipe_handler(int);
	
	struct Basic_factory {

		Basic_factory():
			_local_server(),
			_remote_server(),
			_repository()
		{
			init_signal_handlers();
		}

		virtual ~Basic_factory() {}

		void start() {
			_local_server.start();
			_remote_server.start();
		}

		void stop() {
			_local_server.stop();
			_remote_server.send(new Shutdown);
			_remote_server.stop();
		}

		void wait() {
			_local_server.wait();
			_remote_server.wait();
		}

		Local_server* local_server() { return &_local_server; }
		Remote_server* remote_server() { return &_remote_server; }
		Repository_stack* repository() { return &_repository; }

	private:

		void init_signal_handlers() {
			struct ::sigaction action;
			std::memset(&action, 0, sizeof(struct ::sigaction));
			action.sa_handler = emergency_shutdown;
			::sigaction(SIGTERM, &action, 0);
			::sigaction(SIGINT, &action, 0);
			::feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW);
			::sigaction(SIGFPE, &action, 0);
			ignore_sigpipe();
		}

		void ignore_sigpipe() {
			struct ::sigaction action;
			std::memset(&action, 0, sizeof(struct ::sigaction));
			action.sa_handler = SIG_IGN;
			::sigaction(SIGPIPE, &action, 0);
		}

		Local_server _local_server;
		Remote_server _remote_server;
		Repository_stack _repository;
	};

}

