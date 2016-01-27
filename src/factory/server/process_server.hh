#ifndef FACTORY_SERVER_PIPE_SERVER_HH
#define FACTORY_SERVER_PIPE_SERVER_HH

#include <map>
#include <cassert>

#include <stdx/log.hh>
#include <stdx/front_popper.hh>

#include <sysx/process.hh>
#include <sysx/pipe.hh>
#include <sysx/fildesbuf.hh>
#include <sysx/packetstream.hh>

#include <factory/server/intro.hh>
#include <factory/server/proxy_server.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

namespace factory {
	namespace components {

		template<class T, class Kernels=std::deque<typename Server<T>::kernel_type*>>
		struct Buffered_server: public Managed_object<Server<T>> {

			typedef Managed_object<Server<T>> base_server;
			using typename base_server::kernel_type;
			typedef Kernels pool_type;
			typedef stdx::log<Buffered_server> this_log;

			Buffered_server() = default;

			Buffered_server(Buffered_server&& rhs):
			base_server(std::move(rhs)),
			_buffer(std::move(rhs._buffer))
			{}

			Buffered_server(const Buffered_server&) = delete;
			Buffered_server& operator=(const Buffered_server&) = delete;

			virtual
			~Buffered_server() {
				this->recover_kernels();
				this->delete_remaining_kernels();
			}

			void
			send(kernel_type* kernel) override {
				bool erase_kernel = true;
				if ((kernel->moves_upstream() || kernel->moves_somewhere()) && kernel->identifiable()) {
					_buffer.push_back(kernel);
					erase_kernel = false;
				}
				if (erase_kernel && !kernel->moves_everywhere()) {
					this_log() << "Delete kernel " << *kernel << std::endl;
					delete kernel;
				}
			}

		protected:

			void
			clear_kernel_buffer(kernel_type* k) {
				auto pos = std::find_if(_buffer.begin(), _buffer.end(),
					[k] (kernel_type* rhs) { return *rhs == *k; });
				if (pos != _buffer.end()) {
					this_log() << "Kernel erased " << k->id() << std::endl;
					kernel_type* orig = *pos;
					k->parent(orig->parent());
					k->principal(k->parent());
					delete orig;
					_buffer.erase(pos);
					this_log() << "Buffer size = " << _buffer.size() << std::endl;
				} else {
					this_log() << "Kernel not found " << k->id() << std::endl;
				}
			}

		private:

			void
			recover_kernels() {

				// TODO do we need this?
				// Here failed kernels are written to buffer,
				// from which they must be recovered with recover_kernels().
				// sysx::poll_event ev{socket().fd(), sysx::poll_event::In};
				// handle(ev);

				this_log()
					<< "Kernels left: "
					<< _buffer.size()
					<< std::endl;

				// recover kernels written to output buffer
				using namespace std::placeholders;
				std::for_each(
					stdx::front_popper(_buffer),
					stdx::front_popper_end(_buffer),
					std::bind(&Buffered_server::recover_kernel, this, _1)
				);
			}

			void
			recover_kernel(kernel_type* k) {
				if (k->moves_everywhere()) {
					delete k;
				} else {
					assert(this->root() != nullptr);
					k->from(k->to());
					k->result(Result::ENDPOINT_NOT_CONNECTED);
					k->principal(k->parent());
					this->root()->send(k);
				}
			}

			void
			delete_remaining_kernels() {
				std::for_each(
					stdx::front_popper(_buffer),
					stdx::front_popper_end(_buffer),
					[] (kernel_type* rhs) { delete rhs; }
				);
			}


			pool_type _buffer;

		};

		template<class T>
		struct Process_rserver: public Buffered_server<T> {

//			typedef Managed_object<Server<T>> base_server;
			typedef Buffered_server<T> base_server;
			using typename base_server::kernel_type;
			typedef basic_kernelbuf<sysx::fildesbuf> kernelbuf_type;
			typedef sysx::packetstream stream_type;
			typedef typename kernel_type::app_type app_type;
			typedef stdx::log<Process_rserver> this_log;

			Category
			category() const noexcept override {
				return Category{"proc_rserver"};
			}

			Process_rserver(sysx::pid_type&& child, sysx::two_way_pipe&& pipe):
			_childpid(child),
			_outbuf(std::move(pipe.parent_out())),
			_ostream(&_outbuf),
			_inbuf(std::move(pipe.parent_in())),
			_istream(&_inbuf)
			{
				_inbuf.fd().validate();
				_outbuf.fd().validate();
			}

			Process_rserver(Process_rserver&& rhs):
			base_server(std::move(rhs)),
			_childpid(rhs._childpid),
			_outbuf(std::move(rhs._outbuf)),
			_ostream(&_outbuf),
			_inbuf(std::move(rhs._inbuf)),
			_istream(&_inbuf)
			{
				_inbuf.fd().validate();
				_outbuf.fd().validate();
				this_log() << "root after move ctr " << this->root()
					<< ",this=" << this << std::endl;
			}

			explicit
			Process_rserver(sysx::pipe&& pipe):
			_childpid(sysx::this_process::id()),
			_outbuf(std::move(pipe.out())),
			_ostream(&_outbuf),
			_inbuf(std::move(pipe.in())),
			_istream(&_inbuf)
			{}

			const sysx::pid_type&
			childpid() const {
				return _childpid;
			}

			void
			send(kernel_type* kernel) override {
				if (!kernel->identifiable() && !kernel->moves_everywhere()) {
					kernel->id(this->factory()->factory_generate_id());
					this_log() << "Kernel generate id = " << kernel->id() << std::endl;
				}
				_ostream.begin_packet();
				_ostream << kernel->app() << kernel->from();
				Type<kernel_type>::write_object(*kernel, _ostream);
				_ostream.end_packet();
				stdx::log_func<this_log>(__func__, "kernel", *kernel);
				base_server::send(kernel);
			}

			void
			prepare(sysx::poll_event& event) {
//				if (event.fd() == _outbuf.fd()) {
//					if (_outbuf.dirty()) {
//						event.setev(sysx::poll_event::Out);
//					} else {
//						event.unsetev(sysx::poll_event::Out);
//					}
//				}
			}

			void
			handle(sysx::poll_event& event) {
				stdx::log_func<this_log>(__func__, "ev", event);
				if (event.fd() == _outbuf.fd()) {
					if (_outbuf.dirty()) {
						event.setrev(sysx::poll_event::Out);
					}
					if (event.out() && !event.hup()) {
						_ostream.flush();
						if (!_outbuf.dirty()) {
							this_log() << "Flushed." << std::endl;
						}
					}
				} else {
					assert(event.fd() == _inbuf.fd());
					assert(!event.out() || event.hup());
					if (event.in()) {
						_istream.fill();
						while (_istream.read_packet()) {
							read_and_receive_kernel();
						}
					}
				}
			}

			void
			forward(const Kernel_header& hdr, sysx::packetstream& istr) {
				_ostream.begin_packet();
				_ostream.append_payload(istr);
				_ostream.end_packet();
				stdx::log_func<this_log>(__func__, "hdr", hdr);
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Process_rserver& rhs) {
				return stdx::format_fields(out, "childpid", rhs._childpid);
			}

		private:

			void read_and_receive_kernel() {
				app_type app; sysx::endpoint from;
				_istream >> app >> from;
				this_log() << "recv ok" << std::endl;
				this_log() << "recv app=" << app << std::endl;
				if (app == Application::ROOT || _childpid == sysx::this_process::id()) {
					Type<kernel_type>::read_object(this->factory()->types(), _istream,
						[this,app] (kernel_type* k) {
							receive_kernel(k, app);
						}
					);
				} else {
					Kernel_header hdr;
					hdr.setapp(app);
					this->factory()->remote_server()->forward(hdr, _istream);
				}
			}

			void
			receive_kernel(kernel_type* k, app_type app) {
				k->setapp(app);
				stdx::log_func<this_log>(__func__, "kernel", *k);
				bool ok = true;
				if (k->moves_downstream()) {
					// TODO
					this->clear_kernel_buffer(k);
				} else if (k->principal_id()) {
					kernel_type* p = this->factory()->instances().lookup(k->principal_id());
					if (p == nullptr) {
						k->result(Result::NO_PRINCIPAL_FOUND);
						ok = false;
					}
					k->principal(p);
				}
				if (!ok) {
					return_kernel(k);
				} else {
					this->root()->send(k);
				}
			}

			void return_kernel(kernel_type* k) {
				this_log()
					<< "No principal found for "
					<< *k << std::endl;
				k->principal(k->parent());
				this->send(k);
			}

			sysx::pid_type _childpid;
			kernelbuf_type _outbuf;
			stream_type _ostream;
			kernelbuf_type _inbuf;
			stream_type _istream;

		};

		enum Shared_fildes: sysx::fd_type {
			In  = 100,
			Out = 101
		};

		template<class T>
		struct Process_iserver: public Proxy_server<T,Process_rserver<T>> {

			typedef Proxy_server<T,Process_rserver<T>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			using typename base_server::kernel_pool;
			using typename base_server::server_type;
			using typename base_server::handler_type;

			using base_server::poller;

			typedef sysx::pid_type key_type;
			typedef Process_rserver<T> rserver_type;
			typedef std::map<key_type, rserver_type> map_type;
			typedef stdx::log<Process_iserver> this_log;

			Process_iserver() = default;

			Process_iserver(const Process_iserver&) = delete;
			Process_iserver& operator=(const Process_iserver&) = delete;

			Process_iserver(Process_iserver&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			~Process_iserver() = default;

			Category
			category() const noexcept override {
				return Category{
					"proc_iserver",
					[] () { return new Process_iserver; }
				};
			}

			void
			remove_server(server_type* ptr) override {
				_apps.erase(ptr->childpid());
			}

			void
			process_kernels() override {
				stdx::log_func<this_log>(__func__);
				stdx::front_pop_iterator<kernel_pool> it_end;
				lock_type lock(this->_mutex);
				stdx::for_each_thread_safe(lock,
					stdx::front_popper(this->_kernels), it_end,
					[this] (kernel_type* rhs) { process_kernel(rhs); }
				);
			}

			void
			add(const Application& app) {
				this_log() << "starting app=" << app << std::endl;
				lock_type lock(this->_mutex);
				sysx::two_way_pipe data_pipe;
				const sysx::process& p = _procs.add([&app,this,&data_pipe] () {
					data_pipe.close_in_child();
					data_pipe.remap_in_child(Shared_fildes::In, Shared_fildes::Out);
					data_pipe.validate();
					_globalsem.wait();
					return app.execute();
				});
				sysx::pid_type process_id = p.id();
				data_pipe.close_in_parent();
				data_pipe.validate();
				this_log() << "pipe=" << data_pipe << std::endl;
				sysx::fd_type parent_in = data_pipe.parent_in().get_fd();
				sysx::fd_type parent_out = data_pipe.parent_out().get_fd();
				rserver_type child(p.id(), std::move(data_pipe));
				child.setparent(this);
				assert(child.root() != nullptr);
				this_log() << "starting child process: " << child << std::endl;
				auto result = _apps.emplace(process_id, std::move(child));
				poller().emplace(
					sysx::poll_event{parent_in, sysx::poll_event::In, 0},
					handler_type(&result.first->second)
				);
				poller().emplace(
					sysx::poll_event{parent_out, 0, 0},
					handler_type(&result.first->second)
				);
				_globalsem.notify_one();
			}

			void
			do_run() override {
				std::thread waiting_thread{
					&Process_iserver::wait_for_all_processes_to_finish,
					this
				};
				base_server::do_run();
				waiting_thread.join();
//				this->wait_for_all_processes_to_finish();
			}

			void
			forward(const Kernel_header& hdr, sysx::packetstream& istr) {
				auto result = _apps.find(hdr.app());
				if (result == _apps.end()) {
					throw Error("bad app id", __FILE__, __LINE__, __func__);
				}
				result->second.forward(hdr, istr);
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
				while (apps_are_running()) {
					stdx::unlock_guard<lock_type> g(lock);
					using namespace std::placeholders;
					_procs.wait(std::bind(&Process_iserver::on_process_exit, this, _1, _2));
				}
			}

			void
			on_process_exit(sysx::process& p, sysx::proc_info status) {
				stdx::log_func<this_log>(__func__, "process", p);
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
			sysx::process_group _procs;
			Global_semaphore _globalsem;
		};

		template<class T>
		struct Process_child_server: public Proxy_server<T,Process_rserver<T>> {

			typedef Proxy_server<T,Process_rserver<T>> base_server;
			using typename base_server::kernel_type;
			using typename base_server::mutex_type;
			using typename base_server::lock_type;
			using typename base_server::sem_type;
			using typename base_server::kernel_pool;
			using typename base_server::server_type;
			using typename base_server::handler_type;

			using base_server::poller;

			typedef sysx::pid_type key_type;
			typedef Process_rserver<T> rserver_type;
			typedef std::map<key_type, rserver_type> map_type;
			typedef stdx::log<Process_child_server> this_log;

			Process_child_server():
			_parent(sysx::pipe{Shared_fildes::In, Shared_fildes::Out})
			{}

			Process_child_server(Process_child_server&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			virtual ~Process_child_server() = default;

			Category
			category() const noexcept override {
				return Category{
					"proc_child_server",
					[] () { return new Process_child_server; }
				};
			}

			void
			remove_server(server_type* ptr) override {
				this_log() << "Stopping server because parent died" << std::endl;
				if (!this->stopped()) {
					this->stop();
					this->factory()->stop();
				}
			}

			void
			process_kernels() override {
				stdx::log_func<this_log>(__func__);
				stdx::front_pop_iterator<kernel_pool> it_end;
				lock_type lock(this->_mutex);
				stdx::for_each_thread_safe(lock,
					stdx::front_popper(this->_kernels), it_end,
					[this] (kernel_type* rhs) { process_kernel(rhs); }
				);
			}

			void
			forward(const Kernel_header& hdr, sysx::packetstream& istr) {
				assert(false);
			}

			void
			do_run() override {
				init_server();
				base_server::do_run();
			}

		private:

			void
			init_server() {
				_parent.setparent(this);
				std::cout << "HELLO WORLD " << sysx::poll_event(Shared_fildes::Out, 0, 0)  << std::endl;
				this_log() << "DEBUG=" << sysx::poll_event(Shared_fildes::Out, 0, 0) << std::endl;
				poller().emplace(sysx::poll_event{Shared_fildes::In, sysx::poll_event::In, 0}, handler_type(&_parent));
				poller().emplace(sysx::poll_event(Shared_fildes::Out, 0, 0), handler_type(&_parent));
			}

			void
			process_kernel(kernel_type* k) {
				_parent.send(k);
			}

			rserver_type _parent;
		};


	}
}

namespace stdx {

	template<class T>
	struct type_traits<factory::components::Process_rserver<T>> {
		static constexpr const char*
		short_name() { return "process_rserver"; }
		typedef factory::components::server_category category;
	};

	template<class T>
	struct type_traits<factory::components::Process_iserver<T>> {
		static constexpr const char*
		short_name() { return "process_iserver"; }
		typedef factory::components::server_category category;
	};

	template<class T>
	struct type_traits<factory::components::Process_child_server<T>> {
		static constexpr const char*
		short_name() { return "process_child_server"; }
		typedef factory::components::server_category category;
	};

}

#endif // FACTORY_SERVER_PIPE_SERVER_HH
