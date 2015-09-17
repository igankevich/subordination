#ifndef FACTORY_SERVER_APP_SERVER_HH
#define FACTORY_SERVER_APP_SERVER_HH

#include <thread>
#include <map>
#include <random>

#include <stdx/n_random_bytes.hh>
#include <stdx/unlock_guard.hh>

#include <sysx/semaphore.hh>
#include <sysx/process.hh>
#include <sysx/packetstream.hh>
#include <sysx/shmembuf.hh>

#include <factory/kernelbuf.hh>
#include <factory/managed_object.hh>
#include <factory/server/basic_server.hh>

namespace factory {
	namespace components {

		inline
		std::string
		generate_shmem_id(sysx::pid_type child, sysx::pid_type parent, int stream_no) {
//			static const int MAX_PID = 65536;
//			static const int MAX_STREAMS = 10;
//			return child * MAX_PID * MAX_STREAMS + parent * MAX_STREAMS + stream_no;
			std::ostringstream str;
			str << "/factory-shm-" << parent << '-' << child << '-' << stream_no;
			std::string name = str.str();
			name.shrink_to_fit();
			return name;
		}

		inline
		std::string
		generate_sem_name(sysx::pid_type child, sysx::pid_type parent, char tag) {
			std::ostringstream str;
			str << "/factory-sem-" << parent << '-' << child << '-' << tag;
			std::string name = str.str();
			name.shrink_to_fit();
			return name;
		}

		struct principal_shmembuf: public sysx::shmembuf {
			principal_shmembuf():
			sysx::shmembuf(generate_shmem_id(sysx::this_process::id(),
			sysx::this_process::parent_id(), 0)) {}
		};

		struct app_shmembuf: public sysx::shmembuf {
			app_shmembuf(sysx::pid_type pid):
			sysx::shmembuf(generate_shmem_id(pid, sysx::this_process::id(), 0), 0666) {}
		};

		struct Global_semaphore: public sysx::process_semaphore {

			Global_semaphore():
			sysx::process_semaphore(generate_name(), 0666)
			{}

		private:
			std::string
			generate_name() {
				std::random_device rng;
				uint128_t big_num = stdx::n_random_bytes<uint128_t>(rng);
				std::ostringstream str;
				str << "/factory-sem-" << sysx::this_process::id() << '-' << "app-" << big_num;
				std::string name = str.str();
				name.shrink_to_fit();
				return name;
			}
		};

		constexpr const sysx::signal_type
		APP_SEMAPHORE_SIGNAL = SIGUSR1;

		template<class T>
		struct Principal_server: public Managed_object<Server<T>> {

			typedef Server<T> base_server;
			typedef typename base_server::kernel_type kernel_type;
			typedef sysx::proc process_type;
			typedef basic_kernelbuf<sysx::shmembuf> ibuf_type;
			typedef basic_kernelbuf<sysx::shmembuf> obuf_type;
			typedef sysx::packetstream stream_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;
			typedef sysx::process_semaphore sem_type;
			typedef sysx::signal_semaphore osem_type;
			typedef Application::id_type app_type;
			typedef stdx::log<Principal_server> this_log;

			Principal_server():
			_ibuf(generate_shmem_id(sysx::this_process::id(), sysx::this_process::parent_id(), 1)),
			_obuf(generate_shmem_id(sysx::this_process::id(), sysx::this_process::parent_id(), 0)),
			_istream(&this->_ibuf),
			_ostream(&this->_obuf),
			_isem(generate_sem_name(sysx::this_process::id(), sysx::this_process::parent_id(), 'i')),
			_osem(sysx::this_process::parent_id(), APP_SEMAPHORE_SIGNAL),
			_thread(),
			_app(sysx::this_process::getenv("APP_ID", Application::ROOT))
			{}

			Principal_server(Principal_server&& rhs):
			_ibuf(std::move(rhs._ibuf)),
			_obuf(std::move(rhs._obuf)),
			_istream(std::move(rhs._istream)),
			_ostream(std::move(rhs._ostream)),
			_isem(std::move(rhs._isem)),
			_osem(std::move(rhs._osem)),
			_thread(std::move(rhs._thread)),
			_app(rhs._app)
			{}

			Category
			category() const noexcept override {
				return Category{
					"princ_server",
					[] () { return new Principal_server; }
				};
			}

			void
			start() override {
				base_server::start();
				this_log() << "Principal_server::start()" << std::endl;
				this->_thread = std::thread(std::mem_fn(&Principal_server::serve), this);
			}

			void
			stop() override {
				base_server::stop();
				this->_isem.notify_one();
			}

			void
			wait() override {
				base_server::wait();
				if (this->_thread.joinable()) {
					this->_thread.join();
				}
			}

			void send(kernel_type* k) {
				{
					olock_type lock(_obuf);
					_ostream.begin_packet();
					_ostream << k->app() << k->to();
					this_log() << "send from_app=" << _app
						<< ",krnl=" << *k
						<< std::endl;
					Type<kernel_type>::write_object(*k, _ostream);
					_ostream.end_packet();
				}
				_osem.notify_one();
				this_log() << "notify_one app=" << _app << std::endl;
			}

		private:

			void serve() {
				while (!this->stopped()) {
					this->read_and_send_kernel();
					this->_isem.wait();
				}
			}

			void read_and_send_kernel() {
				ilock_type lock(this->_ibuf);
				while (_istream.read_packet()) {
					app_type app; sysx::endpoint src;
					_istream >> app >> src;
					this_log() << "read_and_send_kernel(): src=" << src
						<< ",rdstate=" << stdx::debug_stream(this->_istream)
						<< std::endl;
					Type<kernel_type>::read_object(this->factory()->types(), this->_istream,
						[this,&src] (kernel_type* k) {
							k->setapp(_app);
							send_kernel(k, src);
						}
					);
				}
			}

			void
			send_kernel(kernel_type* k, const sysx::endpoint& src) {
				k->from(src);
				this_log()
					<< "recv kernel=" << *k
					<< ",rdstate=" << stdx::debug_stream(this->_istream)
					<< std::endl;
				if (k->principal()) {
					kernel_type* p = this->factory()->instances().lookup(k->principal_id());
					if (p == nullptr) {
						this_log() << "principal not found: id=" << k->principal_id() << std::endl;
						k->result(Result::NO_PRINCIPAL_FOUND);
						throw No_principal_found<kernel_type>(k);
					}
					k->principal(p);
				}
				this_log()
					<< "recv2 kernel=" << *k
					<< ",rdstate=" << stdx::debug_stream(this->_istream)
					<< std::endl;
				this->root()->send(k);
			}

			ibuf_type _ibuf;
			obuf_type _obuf;
			stream_type _istream;
			stream_type _ostream;
			sem_type _isem;
			osem_type _osem;
			std::thread _thread;
			app_type _app;
		};

		template<class T>
		struct Sub_Rserver: public Managed_object<Server<T>> {

			typedef Managed_object<Server<T>> base_type;
			typedef Server<T> base_server;
			typedef typename base_server::kernel_type kernel_type;
			typedef sysx::proc process_type;
			typedef basic_kernelbuf<sysx::shmembuf> ibuf_type;
			typedef basic_kernelbuf<sysx::shmembuf> obuf_type;
			typedef std::lock_guard<ibuf_type> ilock_type;
			typedef std::lock_guard<obuf_type> olock_type;
			typedef sysx::process_semaphore sem_type;
			typedef sysx::packetstream stream_type;
			typedef typename Application::id_type app_type;
			typedef stdx::log<Sub_Rserver> this_log;

			explicit
			Sub_Rserver(sysx::pid_type pid):
			_proc(pid),
			_osem(generate_sem_name(pid, sysx::this_process::id(), 'i'), 0666),
			_ibuf(generate_shmem_id(pid, sysx::this_process::id(), 0), 0666),
			_obuf(generate_shmem_id(pid, sysx::this_process::id(), 1), 0666),
			_ostream(&_obuf),
			_istream(&_ibuf)
			{}

			// NB: explicitly call move constructor of a base class
			Sub_Rserver(Sub_Rserver&& rhs):
			base_type(std::move(rhs)),
			_proc(rhs._proc),
			_osem(std::move(rhs._osem)),
			_ibuf(std::move(rhs._ibuf)),
			_obuf(std::move(rhs._obuf)),
			_ostream(&_obuf),
			_istream(&_ibuf)
			{}

			Category
			category() const noexcept override {
				return Category{
					"sub_rserver",
					[] () { return nullptr; }
				};
			}

			void send(kernel_type* k) {
				{
					olock_type lock(_obuf);
					this_log() << "write from " << k->from() << std::endl;
					this_log() << "send kernel " << *k << std::endl;
					_ostream.begin_packet();
					_ostream << k->app() << k->from();
					Type<kernel_type>::write_object(*k, _ostream);
					_ostream.end_packet();
				}
				_osem.notify_one();
			}

			void read_and_send_kernel(app_type this_app) {
				this_log() << "read_and_send_kernel()" << std::endl;
				ilock_type lock(_ibuf);
				while (_istream.read_packet()) {
					app_type app; sysx::endpoint dst;
					_istream >> app >> dst;
					this_log() << "read_and_send_kernel():"
						<< " dst=" << dst
						<< ",app=" << app
						<< std::endl;
					if (app == this_app) {
						this_log() << "forward to self: app=" << app << std::endl;
						forward_internal(_ibuf, sysx::endpoint());
					} else if (app == Application::ROOT) {
						kernel_type* kernel = Type<kernel_type>::read_object(this->factory()->types(), _istream);
						if (kernel) {
							recv_kernel(kernel);
						}
					} else {
						this->app_server()->forward(app, sysx::endpoint(), _ibuf);
					}
				}
			}

			void
			recv_kernel(kernel_type* k) {
				this_log()
					<< "recv kernel=" << *k
					<< ",rdstate=" << stdx::debug_stream(this->_istream)
					<< std::endl;
				if (k->principal()) {
					kernel_type* p = this->factory()->instances().lookup(k->principal()->id());
					if (p == nullptr) {
						k->result(Result::NO_PRINCIPAL_FOUND);
						throw No_principal_found<kernel_type>(k);
					}
					k->principal(p);
				}
				this->root()->send(k);
			}

			template<class X>
			void forward_internal(basic_kernelbuf<X>& buf, const sysx::endpoint& from) {
				{
					olock_type lock(_obuf);
					_ostream.begin_packet();
					append_payload(_obuf, buf);
					_ostream.end_packet();
				}
				_osem.notify_one();
			}

			template<class X>
			void forward(basic_kernelbuf<X>& buf, const sysx::endpoint& from) {
				{
					olock_type lock(_obuf);
					_ostream.begin_packet();
					// TODO
					// _ostream << 
					append_payload(_obuf, buf);
					_ostream.end_packet();
				}
				_osem.notify_one();
			}

			sysx::pid_type
			proc() const {
				return _proc;
			}

		private:
			sysx::pid_type _proc;
			sem_type _osem;
			ibuf_type _ibuf;
			obuf_type _obuf;
			stream_type _ostream;
			stream_type _istream;
		};

		template<class T>
		struct Sub_Iserver: public Standard_server_with_pool<T> {

			typedef Standard_server_with_pool<T> base_server;
			using typename base_server::kernel_type;
			using typename base_server::lock_type;
			typedef Application app_type;
			typedef Sub_Rserver<T> rserver_type;
			typedef typename app_type::id_type key_type;
			typedef std::map<key_type, rserver_type> map_type;
			typedef typename map_type::value_type pair_type;
			typedef stdx::log<Sub_Iserver> this_log;

			Sub_Iserver():
			base_server(1u),
			_signalsem(sysx::this_process::id(), APP_SEMAPHORE_SIGNAL)
			{}

			Category
			category() const noexcept override {
				return Category{
					"sub_iserver",
					[] () { return new Sub_Iserver; }
				};
			}

			void
			add(const app_type& app) {
				if (_apps.count(app.id()) > 0) {
					throw Error("trying to add an existing app",
						__FILE__, __LINE__, __func__);
				}
				this_log() << "starting app=" << app << std::endl;
				lock_type lock(this->_mutex);
				sysx::proc& p = _processes.add([&app,this] () {
					_globalsem.wait();
					return app.execute();
				});
				_pid2app.emplace(p.id(), app.id());
				rserver_type child(p.id());
				child.setparent(this);
				_apps.emplace(app.id(), std::move(child));
				_globalsem.notify_one();
			}
			
			void
			send(kernel_type* k) {
				if (k->moves_everywhere()) {
					std::for_each(_apps.begin(), _apps.end(),
						[k] (pair_type& rhs) {
							rhs.second.send(k);
						}
					);
				} else {
					typename map_type::iterator
					result = _apps.find(k->app());
					if (result == _apps.end()) {
						throw Error("bad app id", __FILE__, __LINE__, __func__);
					}
					result->second.send(k);
				}
			}

			void forward(key_type app, const sysx::endpoint& from, stdx::packetbuf& buf) {
				typename map_type::iterator result = _apps.find(app);
				if (result == _apps.end()) {
					throw Error("bad app id", __FILE__, __LINE__, __func__);
				}
				result->second.forward(buf, from);
			}

			void
			do_run() override {
				std::thread waiting_thread{
					&Sub_Iserver::wait_for_all_processes_to_finish,
					this
				};
				handle_notifications_from_child_procs();
				waiting_thread.join();
			}

			void
			stop() override {
				base_server::stop();
				_signalsem.notify_all();
			}

		private:

			bool
			apps_are_running() const {
				return !(this->stopped() and _apps.empty());
			}

			void
			handle_notifications_from_child_procs() {
				lock_type lock(this->_mutex);
				while (apps_are_running()) {
					_signalsem.wait(lock);
					this_log() << "notify_one pid="
						<< _signalsem.last_notifier()
						<< std::endl;
					auto result = _pid2app.find(_signalsem.last_notifier());
					if (result != _pid2app.end()) {
						auto result2 = _apps.find(result->second);
						if (result2 != _apps.end()) {
							result2->second.read_and_send_kernel(result2->first);
						}
					}
				}
			}

			void
			wait_for_all_processes_to_finish() {
				lock_type lock(this->_mutex);
				while (apps_are_running()) {
					this->_semaphore.wait(lock);
					if (!_apps.empty()) {
						stdx::unlock_guard<lock_type> g(lock);
						using namespace std::placeholders;
						_processes.wait(std::bind(&Sub_Iserver::on_process_exit, this, _1, _2));
					}
				}
				_signalsem.notify_all();
			}

			void
			on_process_exit(sysx::proc& p, sysx::proc_info status) {
				sysx::pid_type pid = p.id();
				lock_type lock(this->_mutex);
				typename map_type::iterator
				result = std::find_if(_apps.begin(), _apps.end(),
					[pid] (const pair_type& rhs) {
						return rhs.second.proc() == pid;
					}
				);
				if (result != this->_apps.end()) {
					this_log() << "finished app="
						<< result->first
						<< ",ret=" << status.exit_code()
						<< ",sig=" << status.term_signal()
						<< std::endl;
					_apps.erase(result);
				}
			}

			map_type _apps;
			sysx::procgroup _processes;
			std::unordered_map<sysx::pid_type, key_type> _pid2app;
			Global_semaphore _globalsem;
			sysx::signal_semaphore _signalsem;
		};

	}
}

namespace stdx {

	template<class T>
	struct type_traits<factory::components::Sub_Iserver<T>> {
		static constexpr const char*
		short_name() { return "sub_iserver"; }
		typedef factory::components::server_category category;
	};

	template<class T>
	struct type_traits<factory::components::Sub_Rserver<T>> {
		static constexpr const char*
		short_name() { return "sub_rserver"; }
		typedef factory::components::server_category category;
	};

	template<class T>
	struct type_traits<factory::components::Principal_server<T>> {
		static constexpr const char*
		short_name() { return "principal_server"; }
		typedef factory::components::server_category category;
	};

}

#endif // FACTORY_SERVER_APP_SERVER_HH
