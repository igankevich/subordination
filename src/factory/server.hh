namespace factory {
	namespace components {
		size_t total_cpus() noexcept { return std::thread::hardware_concurrency(); }
#if defined(FACTORY_NO_CPU_BINDING)
		void thread_affinity(size_t) {}
#else
#if defined(HAVE_CPU_SET_T)
	#if defined(HAVE_SCHED_H)
	#include <sched.h>
	#elif defined(HAVE_SYS_CPUSET_H)
	#include <sys/cpuset.h>
	#endif
	struct CPU {
		typedef ::cpu_set_t Set;

		CPU(size_t cpu) {
			size_t num_cpus = total_cpus();
			cpuset = CPU_ALLOC(num_cpus);
			_size = CPU_ALLOC_SIZE(num_cpus);
			CPU_ZERO_S(size(), cpuset);
			CPU_SET_S(cpu%num_cpus, size(), cpuset);
		}

		~CPU() { CPU_FREE(cpuset); }

		size_t size() const { return _size; }
		Set* set() { return cpuset; }

	private:
		Set* cpuset;
		size_t _size;
	};
#endif
#if defined(HAVE_DECL_PTHREAD_SETAFFINITY_NP)
#include <pthread.h>
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check("pthread_setaffinity_np()",
				::pthread_setaffinity_np(::pthread_self(),
				cpu.size(), cpu.set()));
		}
#elif defined(HAVE_SCHED_H)
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check("sched_setaffinity()",
				::sched_setaffinity(0, cpu.size(), cpu.set()));
		}
#elif defined(HAVE_SYS_PROCESSOR_H)
#include <sys/processor.h>
		void thread_affinity(size_t) {
			::processor_bind(P_LWPID, P_MYID, cpu_id%total_cpus(), 0);
		}
#elif defined(HAVE_SYS_CPUSET_H)
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check("cpuset_setaffinity()",
				::cpuset_setaffinity(CPU_LEVEL_WHICH,
				CPU_WHICH_TID, -1, cpu.size(), cpu.set());
		}
#endif
#endif
	}
}

namespace factory {

	namespace components {

		std::mutex __kernel_delete_mutex;
		std::condition_variable __kernel_semaphore;
		std::atomic<int> __num_rservers(0);

		struct Resident {

			Resident() {}
			virtual ~Resident() {}

			bool stopped() const { return _stopped; }
			void stopped(bool b) { _stopped = b; }

			void wait() {}
			void stop() { stopped(true); }
			void start() {}

		protected:
			void wait_impl() {}
			void stop_impl() {}

		private:
			volatile bool _stopped = false;
		};

		template<class Sub, class Super>
		struct Server_link: public Super {
			void wait() {
				Super::wait();
				static_cast<Sub*>(this)->Sub::wait_impl();
			}
			void stop() {
				Super::stop();
				static_cast<Sub*>(this)->Sub::stop_impl();
			}
		};

		template<class K>
		struct Server: public virtual Resident {

			typedef Server<K> This;
			typedef K Kernel;

			Server() {}
			Server(const This&) = delete;
			Server(This&&) = delete;
			virtual ~Server() {}

			void operator=(This&) = delete;
		};

		template<class Server, class Sub_server>
		struct Iserver: public Server_link<Iserver<Server, Sub_server>, Server> {

			typedef Sub_server Srv;
			typedef Iserver<Server, Sub_server> This;

			Iserver() {}
			Iserver(const This&) = delete;
			Iserver(This&& rhs):
				_upstream(std::move(rhs._upstream)) {}
		
			void add(Srv&& srv) {
				_upstream.emplace_back(srv);
			}

			void add_cpu(size_t cpu) {
				Sub_server srv;
				srv.affinity(cpu);
				_upstream.emplace_back(std::move(srv));
			}

			void wait_impl() {
				Logger<Level::SERVER>() << "Iserver::wait()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::wait));
			}
		
			void stop_impl() {
				Logger<Level::SERVER>() << "Iserver::stop()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::stop));
			}
		
			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << intersperse(rhs._upstream.begin(),
					rhs._upstream.end(), ',');
			}
			
			void start() {
				Logger<Level::SERVER>() << "Iserver::start()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::start));
			}
		
		protected:
			std::vector<Srv> _upstream;
		};

		template<template<class A> class Pool, class Server>
		struct Rserver: public Server_link<Rserver<Pool, Server>, Server> {

			typedef Server Srv;
			typedef typename Server::Kernel Kernel;
			typedef Rserver<Pool, Server> This;

			Rserver():
				_pool(),
				_cpu(0),
				_thread(),
				_mutex(),
				_semaphore()
			{}

			Rserver(This&& rhs):
				_pool(std::move(rhs._pool)),
				_cpu(rhs._cpu),
				_thread(std::move(rhs._thread)),
				_mutex(),
				_semaphore()
			{}

			This& operator=(const This&) = delete;

			virtual ~Rserver() {
				// ensure that kernels inserted without starting
				// a server are deleted
				std::vector<std::unique_ptr<Kernel>> sack;
				delete_all_kernels(std::back_inserter(sack));
			}

			void add() {}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
				_semaphore.notify_one();
			}

			void wait_impl() {
				Logger<Level::SERVER>() << "Rserver::wait()" << std::endl;
				if (_thread.joinable()) {
					_thread.join();
				}
			}

			void stop_impl() {
				Logger<Level::SERVER>() << "Rserver::stop_impl()" << std::endl;
				_semaphore.notify_all();
			}

			void start() {
				Logger<Level::SERVER>() << "Rserver::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "rserver " << rhs._cpu;
			}

			void affinity(size_t cpu) { _cpu = cpu; }

		protected:

			void serve() {
				++__num_rservers;
				thread_affinity(_cpu);
				while (!this->stopped()) {
					if (!_pool.empty()) {
						std::unique_lock<std::mutex> lock(_mutex);
						Kernel* kernel = _pool.front();
						_pool.pop();
						lock.unlock();
						this->process_kernel(kernel);
//						kernel->act();
					} else {
						wait_for_a_kernel();
					}
				}
				// Recursively collect kernel pointers to the sack
				// and delete them all at once. Collection process
				// is fully serial to prevent multiple deletions
				// and access to unitialised values.
				std::unique_lock<std::mutex> lock(__kernel_delete_mutex);
				std::vector<std::unique_ptr<Kernel>> sack;
				delete_all_kernels(std::back_inserter(sack));
				// simple barrier for all threads participating in deletion
				if (--__num_rservers <= 0) {
					__kernel_semaphore.notify_all();
				}
				__kernel_semaphore.wait(lock, [] () {
					return __num_rservers <= 0;
				});
				// destructors of scoped variables
				// will destroy all kernels automatically
			}

		private:

			void wait_for_a_kernel() {
				std::unique_lock<std::mutex> lock(_mutex);
				_semaphore.wait(lock, [this] {
					return !_pool.empty() || this->stopped();
				});
			}

			template<class It>
			void delete_all_kernels(It it) {
				while (!_pool.empty()) {
					_pool.front()->mark_as_deleted(it);
					_pool.pop();
				}
			}

			Pool<Kernel*> _pool;
			size_t _cpu = 0;
		
			std::thread _thread;
			std::mutex _mutex;
			std::condition_variable _semaphore;
		};

		template<class Server>
		struct Tserver: public Server_link<Tserver<Server>, Server> {

			typedef Server Srv;
			typedef typename Server::Kernel Kernel;
			typedef Tserver<Server> This;

			struct Compare_time {
				bool operator()(const Kernel* lhs, const Kernel* rhs) const {
					return (lhs->timed() && !rhs->timed()) || lhs->at() > rhs->at();
				}
			};

			typedef std::priority_queue<Kernel*, std::vector<Kernel*>, Compare_time> Pool;

			Tserver():
				_pool(),
				_cpu(0),
				_thread(),
				_mutex(),
				_semaphore()
			{}

			virtual ~Tserver() {
				std::unique_lock<std::mutex> lock(_mutex);
				while (!_pool.empty()) {
					delete _pool.top();
					_pool.pop();
				}
			}

			void add() {}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
				_semaphore.notify_one();
			}

			void wait_impl() {
				Logger<Level::SERVER>() << "Tserver::wait()" << std::endl;
				if (_thread.joinable()) {
					_thread.join();
				}
			}

			void stop_impl() {
				Logger<Level::SERVER>() << "Tserver::stop_impl()" << std::endl;
				_semaphore.notify_all();
			}

			void start() {
				Logger<Level::SERVER>() << "Tserver::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "tserver " << rhs._cpu;
			}

			void affinity(size_t cpu) { _cpu = cpu; }

		protected:

			void serve() {
				thread_affinity(_cpu);
				while (!this->stopped()) {
					if (!_pool.empty()) {
						std::unique_lock<std::mutex> lock(_mutex);
						Kernel* kernel = _pool.top();
						bool run = true;
						if (kernel->at() > Kernel::Clock::now() && kernel->timed()) {
							using namespace std::chrono;
							run = _semaphore.wait_until(lock, kernel->at(),
								[this] { return this->stopped(); });
						}
						if (run) {
							_pool.pop();
							lock.unlock();
							factory_send(kernel);
						}
					} else {
						wait_for_a_kernel();
					}
				}
			}

		private:

			void wait_for_a_kernel() {
				std::unique_lock<std::mutex> lock(_mutex);
				_semaphore.wait(lock, [this] {
					return !_pool.empty() || this->stopped();
				});
			}

			Pool _pool;
			size_t _cpu;
		
			std::thread _thread;
			std::mutex _mutex;
			std::condition_variable _semaphore;
		};

//		template<template<class A> class Pool, class Server>
//		struct App_server: public Server_link<App_server<Pool, Server>, Server> {
//
//			typedef typename Server::Kernel Kernel;
//			
//			void send(Kernel* k) {
//			}
//
//			void stop_impl() { _procs.stop(); }
//
//			void wait_impl() { _procs.wait(); }
//
//		private:
//			Process_group _procs;
//		};

	}
}
namespace factory {

	namespace components {

		template<template<class X> class Mobile, class Type>
		struct Shutdown: public Mobile<Shutdown<Mobile, Type>> {
			explicit Shutdown(bool f=false): force(f) {}
			void act() {
				Logger<Level::COMPONENT>() << "broadcasting shutdown message" << std::endl;
				bool f = this->force;
				delete this;
				components::factory_stop(f);
			}
//			void react() {}
			void write_impl(Foreign_stream&) {}
			void read_impl(Foreign_stream&) {}
			static void init_type(Type* t) {
				t->id(123);
				t->name("Shutdown");
			}
		private:
			bool force = false;
		};

		
		template<
			class Local_server,
			class Remote_server,
			class External_server,
			class Timer_server,
			class Repository_stack,
			class Shutdown
		>
		struct Basic_factory: public Stoppable {

			Basic_factory():
				_local_server(),
				_remote_server(),
				_ext_server(),
				_timer_server(),
				_repository()
			{
				init_signal_handlers();
			}

			virtual ~Basic_factory() {}

			void start() {
				_local_server.start();
				_remote_server.start();
				_ext_server.start();
				_timer_server.start();
			}

			void stop_now() {
				_local_server.stop();
				_remote_server.stop();
				_ext_server.stop();
				_timer_server.stop();
			}

			void stop() {
				_remote_server.send(new Shutdown);
				_ext_server.send(new Shutdown);
				Shutdown* s = new Shutdown(true);
				s->after(std::chrono::milliseconds(500));
				_timer_server.send(s);
			}

			void wait() {
				_local_server.wait();
				_remote_server.wait();
				_ext_server.wait();
				_timer_server.wait();
			}

			Local_server* local_server() { return &_local_server; }
			Remote_server* remote_server() { return &_remote_server; }
			External_server* ext_server() { return &_ext_server; }
			Timer_server* timer_server() { return &_timer_server; }
			Repository_stack* repository() { return &_repository; }

		private:

			void init_signal_handlers() {
				_ptr_for_sighandler = this;
				Action shutdown(emergency_shutdown);
				this_process::bind_signal(SIGTERM, shutdown);
				this_process::bind_signal(SIGINT, shutdown);
				this_process::bind_signal(SIGPIPE, Action(SIG_IGN));
#ifndef FACTORY_NO_BACKTRACE
				this_process::bind_signal(SIGSEGV, Action(print_stack_trace));
#endif
			}

			static void emergency_shutdown(int sig) noexcept {
				Basic_factory* factory = _ptr_for_sighandler;
				if (factory) { factory->stop(); }
				static int num_calls = 0;
				static const int MAX_CALLS = 3;
				num_calls++;
				std::clog << "Ctrl-C shutdown." << std::endl;
				if (num_calls >= MAX_CALLS) {
					std::clog << "MAX_CALLS reached. Aborting." << std::endl;
					std::abort();
				}
			}

			static void print_stack_trace(int) {
				throw Error("segmentation fault", __FILE__, __LINE__, __func__);
			}

			Local_server _local_server;
			Remote_server _remote_server;
			External_server _ext_server;
			Timer_server _timer_server;
			Repository_stack _repository;

			static Basic_factory* _ptr_for_sighandler;
		};

		template<class A, class B, class C, class D, class E, class F>
		Basic_factory<A,B,C,D,E,F>* Basic_factory<A,B,C,D,E,F>::_ptr_for_sighandler = nullptr;

	}

}

