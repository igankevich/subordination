namespace factory {
	namespace components {
		inline size_t total_cpus() noexcept { return std::thread::hardware_concurrency(); }
		void thread_affinity(size_t);
	}
}

namespace factory {

	namespace components {

		extern std::mutex __kernel_delete_mutex;
		void register_server();
		void global_barrier(std::unique_lock<std::mutex>&);

		struct Resident {

			Resident() {}
			virtual ~Resident() {}

			bool stopped() const { return _stopped; }
			void stopped(bool b) { _stopped = b; }

			void wait() {}
			virtual void stop() { stopped(true); }
			virtual void stop_now() {}
			// TODO: boilerplate :(
			virtual Endpoint addr() const { return Endpoint(); }
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

			Server() = default;
			virtual ~Server() {}
			void operator=(This&) = delete;

			virtual void send(Kernel*) = 0;
			This* parent() const { return this->_parent; }
			void setparent(This* rhs) { this->_parent = rhs; }
			This* root() { return this->_parent ? this->_parent->root() : this; }

		private:
			This* _parent = nullptr;
		};

		template<class Server, class Sub_server>
		struct Iserver: public Server_link<Iserver<Server, Sub_server>, Server> {

			typedef Sub_server Srv;
			typedef Iserver<Server, Sub_server> This;

			Iserver() {}
			Iserver(const This&) = delete;
			Iserver(This&& rhs): _upstream(std::move(rhs._upstream)) {
				std::for_each(
					_upstream.begin(),
					_upstream.end(),
					std::bind(std::mem_fn(&Srv::setparent), this));
			}
		
			void add(Srv&& srv) {
				srv.setparent(this);
				_upstream.emplace_back(srv);
			}

			void add_cpu(size_t cpu) {
				Sub_server srv;
				srv.affinity(cpu);
				srv.setparent(this);
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
				register_server();
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
				global_barrier(lock);
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
							run = _semaphore.wait_until(lock, kernel->at(),
								[this] { return this->stopped(); });
						}
						if (run) {
							_pool.pop();
							lock.unlock();
							this->parent()->send(kernel);
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
				components::stop_all_factories(f);
			}
//			void react() {}
			void write_impl(packstream&) {}
			void read_impl(packstream&) {}
			static void init_type(Type* t) {
				t->id(123);
				t->name("Shutdown");
			}
		private:
			bool force = false;
		};


		void stop_all_factories(bool now);
		void print_all_endpoints(std::ostream& out);
		void register_factory(Resident* factory);

		template<
			class Local_server,
			class Remote_server,
			class External_server,
			class Timer_server,
			class App_server,
			class Principal_server,
			class Repository_stack,
			class Shutdown
		>
		struct Basic_factory: public Server<typename Local_server::Kernel> {

			typedef typename Local_server::Kernel Kernel;
			enum struct Role {
				Principal,
				Subordinate
			};

			Basic_factory():
				_local_server(),
				_remote_server(),
				_ext_server(),
				_timer_server(),
				_app_server(),
				_principal_server(),
				_repository()
			{
				init_parents();
				register_factory(this);
			}

			virtual ~Basic_factory() {}

			void start() {
				_local_server.start();
				_remote_server.start();
				_ext_server.start();
				_timer_server.start();
				_app_server.start();
				if (_role == Role::Subordinate) {
					_principal_server.start();
				}
			}

			void stop_now() {
				_local_server.stop();
				_remote_server.stop();
				_ext_server.stop();
				_timer_server.stop();
				_app_server.stop();
				if (_role == Role::Subordinate) {
					_principal_server.stop();
				}
			}

			void stop() {
				_remote_server.send(new Shutdown);
				_ext_server.send(new Shutdown);
				_app_server.send(new Shutdown);
				if (_role == Role::Subordinate) {
					_principal_server.send(new Shutdown);
				}
				Shutdown* s = new Shutdown(true);
				s->after(std::chrono::milliseconds(500));
				_timer_server.send(s);
			}

			void wait() {
				_local_server.wait();
				_remote_server.wait();
				_ext_server.wait();
				_timer_server.wait();
				_app_server.wait();
				if (_role == Role::Subordinate) {
					_principal_server.wait();
				}
			}

			void send(Kernel* k) { this->_local_server.send(k); }

			Local_server* local_server() { return &_local_server; }
			Remote_server* remote_server() { return &_remote_server; }
			External_server* ext_server() { return &_ext_server; }
			Timer_server* timer_server() { return &_timer_server; }
			App_server* app_server() { return &_app_server; }
			Principal_server* principal_server() { return &_principal_server; }
			Repository_stack* repository() { return &_repository; }

			Endpoint addr() const { return _remote_server.server_addr(); }

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

			Local_server _local_server;
			Remote_server _remote_server;
			External_server _ext_server;
			Timer_server _timer_server;
			App_server _app_server;
			Principal_server _principal_server;
			Repository_stack _repository;
			Role _role = Role::Principal;
		};

	}

}

