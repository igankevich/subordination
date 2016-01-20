#ifndef FACTORY_SERVER_PIPE_SERVER_HH
#define FACTORY_SERVER_PIPE_SERVER_HH

#include <map>

#include <stdx/log.hh>
#include <stdx/front_popper.hh>

#include <sysx/process.hh>
#include <sysx/pipe.hh>
#include <sysx/fildesbuf.hh>

#include <factory/server/intro.hh>
#include <factory/server/proxy_server.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

namespace factory {
	namespace components {

		template<class T>
		struct Pipe_rserver: public Managed_object<Server<T>> {

			typedef Managed_object<Server<T>> base_server;
			using typename base_server::kernel_type;
			typedef basic_kernelbuf<sysx::fildesbuf> kernelbuf_type;
			typedef Kernel_stream<kernel_type> stream_type;

			Pipe_rserver(sysx::process&& child, sysx::two_way_pipe&& pipe):
			_childproc(child),
			_datapipe(pipe),
			_outbuf(pipe.parent_out()),
			_ostream(&_outbuf)
			{}

			const sysx::process&
			childproc() const {
				return _childproc;
			}

			void
			send(kernel_type* kernel) override {
				_ostream << *kernel;
			}

		private:

			sysx::process _childproc;
			sysx::two_way_pipe _datapipe;
			kernelbuf_type _outbuf;
			stream_type _ostream;

		};

		template<class T>
		struct Pipe_iserver: public Proxy_server<T,Pipe_rserver<T>> {

			typedef Proxy_server<T,Pipe_rserver<T>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			using typename base_server::kernel_pool;
			using typename base_server::server_type;

			using base_server::poller;

			typedef Application app_type;
			typedef sysx::pid_type key_type;
			typedef Pipe_rserver<T> rserver_type;
			typedef std::map<key_type, rserver_type> map_type;
			typedef stdx::log<Pipe_iserver> this_log;

			Pipe_iserver() = default;

			Pipe_iserver(Pipe_iserver&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			virtual ~Pipe_iserver() = default;

			Category
			category() const noexcept override {
				return Category{
					"pipe_iserver",
					[] () { return new Pipe_iserver; }
				};
			}

			void
			remove_server(server_type* ptr) override {
				_apps.erase(ptr->childproc().id());
			}

			void
			process_kernels() override {
				this_log() << "Pipe_iserver::process_kernels()" << std::endl;
				stdx::front_pop_iterator<kernel_pool> it_end;
				lock_type lock(this->_mutex);
				stdx::for_each_thread_safe(lock,
					stdx::front_popper(this->_kernels), it_end,
					[this] (kernel_type* rhs) { process_kernel(rhs); }
				);
			}

			void
			add(const app_type& app) {
				if (_apps.count(app.id()) > 0) {
					throw Error("trying to add an existing app",
						__FILE__, __LINE__, __func__);
				}
				this_log() << "starting app=" << app << std::endl;
				lock_type lock(this->_mutex);
				sysx::two_way_pipe data_pipe;
				sysx::process& p = _processes.add([&app,this,&data_pipe] () {
					data_pipe.close_in_child();
					data_pipe.remap_in_child(STDIN_FILENO, STDOUT_FILENO);
					_globalsem.wait();
					return app.execute();
				});
				data_pipe.close_in_parent();
				rserver_type child(std::move(p), std::move(data_pipe));
				child.setparent(this);
				_apps.emplace(app.id(), std::move(child));
//				poller().emplace(sysx::poll_event{fd, events, revents},
//					handler_type(&result.first->second));
				_globalsem.notify_one();
			}

			void
			do_run() override {
				base_server::do_run();
				this->wait_for_all_processes_to_finish();
			}

		private:

			void
			process_kernel(kernel_type* k) {
				typedef typename map_type::value_type value_type;
				if (k->moves_everywhere()) {
					std::for_each(_apps.begin(), _apps.end(),
						[k] (value_type& rhs) {
							rhs.second.send(k);
						}
					);
				} else {
					auto result = _apps.find(k->app());
					if (result == _apps.end()) {
						throw Error("bad app id", __FILE__, __LINE__, __func__);
					}
					result->second.send(k);
				}
			}

			bool
			apps_are_running() const {
				return !(this->stopped() and _apps.empty());
			}

			void
			wait_for_all_processes_to_finish() {
				lock_type lock(this->_mutex);
				while (!_apps.empty()) {
					stdx::unlock_guard<lock_type> g(lock);
					using namespace std::placeholders;
					_processes.wait(std::bind(&Pipe_iserver::on_process_exit, this, _1, _2));
				}
			}

			void
			on_process_exit(sysx::process& p, sysx::proc_info status) {
				lock_type lock(this->_mutex);
				auto result = _apps.find(p.id());
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
			sysx::process_group _processes;
			Global_semaphore _globalsem;
		};

	}
}

#endif // FACTORY_SERVER_PIPE_SERVER_HH
