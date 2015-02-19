#ifdef __linux__
// Locking thread to a single CPU.
#include <sched.h>
// Getting number of CPUs.
#include <unistd.h>
namespace {
	int total_cpus() { return ::sysconf(_SC_NPROCESSORS_ONLN); }
	int thread_affinity() { return ::sched_getcpu(); }
	void thread_affinity(int cpu) {
		if (cpu != -1) {
			int num_cpus = total_cpus();
			::cpu_set_t* cpuset = CPU_ALLOC(num_cpus);
			std::size_t cpuset_size = CPU_ALLOC_SIZE(num_cpus);
			CPU_SET_S(cpu%num_cpus, cpuset_size, cpuset);
			::sched_setaffinity(0, cpuset_size, cpuset);
			CPU_FREE(cpuset);
		}
	}
}
#endif

#ifdef __sun__
#include <sys/processor.h>
// Getting number of CPUs.
#include <unistd.h>
namespace {
	void thread_affinity(int) {
//		int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
//		processor_bind(P_LWPID, P_MYID, cpu_id%num_cpus, NULL);
	}
	int thread_affinity() {
		processorid_t id;
		processor_bind(P_LWPID, P_MYID, PBIND_QUERY, &id);
		return id;
	}
	int total_cpus() { return ::sysconf(_SC_NPROCESSORS_ONLN); }
}
#endif

namespace {

	int total_threads() {
		const char* threads = ::getenv("NUM_THREADS");
		int t = total_cpus();
		if (threads != NULL) {
			std::stringstream tmp;
			tmp << threads;
			if (!(tmp >> t) || t < 0 || t > total_cpus()) {
				t = total_cpus();
				std::clog << "Bad NUM_THREADS value: " << threads << std::endl;
			}
		}
//		std::clog << "threads = " <<  t << std::endl;
		return t;
	}

	int total_vthreads() {
		const char* threads = ::getenv("NUM_VTHREADS");
		int t = 1;
		if (threads != NULL) {
			std::stringstream tmp;
			tmp << threads;
			if (!(tmp >> t) || t < 0) {
				t = 1;
				std::clog << "Bad NUM_VTHREADS value: " << threads << std::endl;
			}
		}
		return t;
	}

}


namespace factory {

	namespace components {

		template<class T>
		void delete_object(T* ob) { delete ob; }

		struct Resident {

			Resident(): _stopped(false) {}

			bool stopped() const { return _stopped; }
			void stopped(bool b) { _stopped = b; }

			void wait() {}
			void stop() { stopped(true); }
			void start() {}

		protected:
			void wait_impl() {}
			void stop_impl() {}

		private:
			volatile bool _stopped;
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
			virtual ~Server() {}

			void operator=(This&) = delete;
		};

		template<class Server, class Sub_server>
		struct Iserver: public Server_link<Iserver<Server, Sub_server>, Server> {

			typedef Sub_server Srv;
			typedef Iserver<Server, Sub_server> This;
		
			void add(Srv* srv) { _upstream.push_back(srv); }
			void add(int cpu) {
				Sub_server* srv = new Sub_server;
				srv->affinity(cpu);
				_upstream.push_back(srv);
			}
		
			virtual ~Iserver() {
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					delete_object<Srv>);
			}
		
			void wait_impl() {
				std::clog << "Iserver::wait()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::wait));
			}
		
			void stop_impl() {
				std::clog << "Iserver::stop()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::stop));
			}
		
			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				out << "iserver {";
				std::ostream_iterator<Srv*> out_it(out, ", ");
				if (rhs._upstream.size() > 1) {
					std::copy(rhs._upstream.begin(), rhs._upstream.end()-1, out_it);
				}
				if (rhs._upstream.size() > 0) {
					out << rhs._upstream[rhs._upstream.size()-1];
				}
				out << '}';
				return out;
			}
			
			void start() {
				std::clog << "Iserver::start()" << std::endl;
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::mem_fn(&Srv::start));
			}
		
		protected:
			std::vector<Srv*> _upstream;
		};

		template<template<class A> class Pool, class Server>
		struct Rserver: public Server_link<Rserver<Pool, Server>, Server> {

			typedef Server Srv;
			typedef typename Server::Kernel Kernel;
			typedef Rserver<Pool, Server> This;

			Rserver():
				_pool(),
				_cpu(0),
				_thread()
			{}

			virtual ~Rserver() {
				std::unique_lock<std::mutex> lock(_mutex);
				while (!_pool.empty()) {
					Kernel* kernel = _pool.front();
					_pool.pop();
					delete kernel;
				}
			}

			void add() {}

			void send(Kernel* kernel) {
				std::unique_lock<std::mutex> lock(_mutex);
				_pool.push(kernel);
				_semaphor.notify_one();
			}

			void wait_impl() {
				std::clog << "Rserver::wait()" << std::endl;
				if (_thread.joinable()) {
					_thread.join();
				}
			}

			void stop_impl() {
				std::clog << "Rserver::stop_impl()" << std::endl;
				_semaphor.notify_all();
			}

			void start() {
				std::clog << "Rserver::start()" << std::endl;
				_thread = std::thread([this] { this->serve(); });
			}

			friend std::ostream& operator<<(std::ostream& out, const This* rhs) {
				return operator<<(out, *rhs);
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "rserver " << rhs._cpu;
			}

			void affinity(int cpu) { _cpu = cpu; }

		protected:

			void serve() {
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
			}

		private:

			void wait_for_a_kernel() {
				std::unique_lock<std::mutex> lock(_mutex);
				_semaphor.wait(lock, [this] {
					return !_pool.empty() || this->stopped();
				});
			}

			Pool<Kernel*> _pool;
			int _cpu;
		
			std::mutex _mutex;
			std::condition_variable _semaphor;
			std::thread _thread;
		};

		template<class Server>
		class Server_server: public Server_link<Server_server<Server>, Server> {
		public:

			typedef Server Srv;
			typedef typename Server::Kernel Kernel;
			typedef Server_server<Server> This;
			typedef std::pair<Kernel*, std::thread*> Svc;

			Server_server():
				_services(),
				_queue_mutex(),
				_control_mutex(),
				_semaphore(),
				_cpu(0)
			{}

			virtual ~Server_server() {
				std::unique_lock<std::mutex> lock(_queue_mutex);
				for (size_t i=0; i<_services.size(); ++i) {
					delete _services[i].second;
					if (_services[i].first != nullptr) {
						delete _services[i].first;
					}
				}
			}

			void add() {}

			void send(Kernel* kernel) {
				std::clog << "Server_server::send()" << std::endl;
				std::unique_lock<std::mutex> lock(_queue_mutex);
				const size_t n = _services.size();
				_services.push_back(std::make_pair(kernel, new std::thread([this, kernel, n] {
					thread_affinity(_cpu);
					kernel->act();
					delete kernel;
					_services[n].first = nullptr;
				})));
			}

			void wait_impl() {
				std::clog << "Server_server::wait()" << std::endl;
				std::unique_lock<std::mutex> lock(_control_mutex);
				_semaphore.wait(lock, [this] { return this->stopped(); });
				std::for_each(_services.begin(), _services.end(), [] (Svc svc) {
					std::thread* t = svc.second;
					if (t->joinable()) {
						t->join();
					}
				});
			}

			void stop_impl() {
				std::clog << "Server_server::stop()" << std::endl;
				std::unique_lock<std::mutex> lock(_control_mutex);
				std::for_each(_services.begin(), _services.end(), [] (Svc svc) {
					svc.first->stop();
				});
				_semaphore.notify_all();
			}

			void affinity(int cpu) { _cpu = cpu; }

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << "srvserver " << rhs._cpu;
			}

		private:
			std::vector<Svc> _services;
			std::mutex _queue_mutex;
			std::mutex _control_mutex;
			std::condition_variable _semaphore;
			int _cpu;
		};

		template<class Server=Null, class ... Next_server>
		struct Server_stack: public Server, public Server_stack<Next_server...> {

			typedef Server_stack<Next_server...> Next;
			typedef Server_stack<Server, Next_server...> This;

			using Server::send;
			using Next::send;
			using Server::add;
			using Next::add;

//			typedef typename Server::Kernel Kernel;
//			void send(Kernel* kernel) {
//				Server::send(kernel);
//			}

			void start() {
				Server::start();
				Next::start();
			}

			void stop() {
				Server::stop();
				Next::stop();
			}

			void wait() {
				Server::wait();
				Next::wait();
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out
					<< static_cast<const Server&>(rhs)
					<< '\n'
					<< static_cast<const Next&>(rhs);
			}

		};

		template<>
		struct Server_stack<Null>: public virtual Resident {

			typedef Server_stack<Null> This;

			void send() {}
			void add() {}

			friend std::ostream& operator<<(std::ostream& out, const This&) {
				return out;
			}

		};

	}
}
