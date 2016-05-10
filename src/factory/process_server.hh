#ifndef FACTORY_PROCESS_SERVER_HH
#define FACTORY_PROCESS_SERVER_HH

#include <map>
#include <cassert>

#include <stdx/log.hh>
#include <stdx/iterator.hh>

#include <sys/process.hh>
#include <sys/pipe.hh>
#include <sys/fildesbuf.hh>
#include <sys/packetstream.hh>

#include <factory/bits/server_category.hh>
#include <factory/basic_server.hh>
#include <factory/proxy_server.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>
#include <factory/result.hh>
#include <factory/kernel.hh>

namespace factory {

	template<class T, class Router, class Kernels=std::deque<T*>>
	struct Buffered_server: public Server_base {

		typedef Server_base base_server;
		typedef T kernel_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef stdx::log<Buffered_server> this_log;

		explicit
		Buffered_server(router_type& router):
		_router(router)
		{}

		Buffered_server(Buffered_server&& rhs):
		base_server(std::move(rhs)),
		_buffer(std::move(rhs._buffer)),
		_router(rhs._router)
		{}

		Buffered_server(const Buffered_server&) = delete;
		Buffered_server& operator=(const Buffered_server&) = delete;

		virtual
		~Buffered_server() {
			this->recover_kernels();
			this->delete_remaining_kernels();
		}

		void
		send(kernel_type* kernel) {
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
			// sys::poll_event ev{socket().fd(), sys::poll_event::In};
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
				// TODO 2016-05-06 fix this
				// assert(this->root() != nullptr);
				k->from(k->to());
				k->result(Result::endpoint_not_connected);
				k->principal(k->parent());
				_router.send_local(k);
			}
		}

		void
		delete_remaining_kernels() {
			stdx::delete_each(
				stdx::front_popper(_buffer),
				stdx::front_popper_end(_buffer)
			);
		}


		pool_type _buffer;

	protected:

		router_type& _router;

	};

	template<class T, class Router>
	struct Process_rserver: public Buffered_server<T,Router> {

		typedef Buffered_server<T,Router> base_server;
		using typename base_server::kernel_type;
		using typename base_server::router_type;
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
		typedef sys::packetstream stream_type;
		typedef typename kernel_type::app_type app_type;
		typedef stdx::log<Process_rserver> this_log;

		using base_server::_router;

		Process_rserver(sys::pid_type&& child, sys::two_way_pipe&& pipe, router_type& router):
		base_server(router),
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
		}

		explicit
		Process_rserver(sys::pipe&& pipe, router_type& router):
		base_server(router),
		_childpid(sys::this_process::id()),
		_outbuf(std::move(pipe.out())),
		_ostream(&_outbuf),
		_inbuf(std::move(pipe.in())),
		_istream(&_inbuf)
		{}

		const sys::pid_type&
		childpid() const {
			return _childpid;
		}

		void
		send(kernel_type* kernel) {
			/*
			TODO we need IDs only when forwarding kernels to other hosts
			if (!kernel->identifiable() && !kernel->moves_everywhere()) {
				kernel->id(this->factory()->factory_generate_id());
				this_log() << "Kernel generate id = " << kernel->id() << std::endl;
			}
			*/
			_ostream.begin_packet();
			_ostream << kernel->app() << kernel->from();
			const Type* type = ::factory::types.lookup(typeid(*kernel));
			if (not type) {
				throw Bad_type(
					"no type is defined for the kernel",
					{__FILE__, __LINE__, __func__},
					kernel->id()
				);
			}
			_ostream << type->id();
			kernel->write(_ostream);
			_ostream.end_packet();
			stdx::log_func<this_log>(__func__, "kernel", *kernel);
			base_server::send(kernel);
		}

		void
		prepare(sys::poll_event& event) {
//				if (event.fd() == _outbuf.fd()) {
//					if (_outbuf.dirty()) {
//						event.setev(sys::poll_event::Out);
//					} else {
//						event.unsetev(sys::poll_event::Out);
//					}
//				}
		}

		void
		handle(sys::poll_event& event) {
			stdx::log_func<this_log>(__func__, "ev", event);
			if (event.fd() == _outbuf.fd()) {
				if (_outbuf.dirty()) {
					event.setrev(sys::poll_event::Out);
				}
				if (event.out() && !event.hup()) {
					_ostream.flush();
					if (!_outbuf.dirty()) {
						this_log() << "Flushed." << std::endl;
					}
				}
			} else {
				assert(
					event.fd() == _inbuf.fd()
					or !_inbuf.fd()
					or !event.bad_fd()
				);
				assert(!event.out() || event.hup());
				if (event.in()) {
					_istream.sync();
					while (_istream.read_packet()) {
						read_and_receive_kernel();
					}
				}
			}
		}

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
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
			app_type app; sys::endpoint from;
			_istream >> app >> from;
			this_log() << "recv ok" << std::endl;
			this_log() << "recv app=" << app << std::endl;
			if (app == Application::ROOT || _childpid == sys::this_process::id()) {
				Types::read_object(factory::types, _istream,
					[this,app] (void* rhs) {
						kernel_type* k = static_cast<kernel_type*>(rhs);
						receive_kernel(k, app);
					}
				);
			} else {
				Kernel_header hdr;
				hdr.setapp(app);
				_router.forward(hdr, _istream);
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
				kernel_type* p = ::factory::instances.lookup(k->principal_id());
				if (p == nullptr) {
					k->result(Result::no_principal_found);
					ok = false;
				}
				k->principal(p);
			}
			if (!ok) {
				return_kernel(k);
			} else {
				_router.send_local(k);
			}
		}

		void return_kernel(kernel_type* k) {
			this_log()
				<< "No principal found for "
				<< *k << std::endl;
			k->principal(k->parent());
			this->send(k);
		}

		sys::pid_type _childpid;
		kernelbuf_type _outbuf;
		stream_type _ostream;
		kernelbuf_type _inbuf;
		stream_type _istream;

	};

	enum Shared_fildes: sys::fd_type {
		In  = 100,
		Out = 101
	};

	template<class T, class Router>
	struct Process_iserver: public Proxy_server<T,Process_rserver<T,Router>> {

		typedef Process_rserver<T,Router> rserver_type;
		typedef Proxy_server<T,rserver_type> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;
		using typename base_server::server_type;
		using typename base_server::handler_type;
		typedef typename rserver_type::router_type router_type;

		using base_server::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type, rserver_type> map_type;
		typedef stdx::log<Process_iserver> this_log;

		Process_iserver() = default;

		Process_iserver(const Process_iserver&) = delete;
		Process_iserver& operator=(const Process_iserver&) = delete;

		Process_iserver(Process_iserver&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		~Process_iserver() = default;

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
			sys::two_way_pipe data_pipe;
			const sys::process& p = _procs.emplace([&app,this,&data_pipe] () {
				data_pipe.close_in_child();
				data_pipe.remap_in_child(Shared_fildes::In, Shared_fildes::Out);
				data_pipe.validate();
				return app.execute();
			});
			sys::pid_type process_id = p.id();
			data_pipe.close_in_parent();
			data_pipe.validate();
			this_log() << "pipe=" << data_pipe << std::endl;
			sys::fd_type parent_in = data_pipe.parent_in().get_fd();
			sys::fd_type parent_out = data_pipe.parent_out().get_fd();
			rserver_type child(p.id(), std::move(data_pipe), _router);
			// child.setparent(this);
			// assert(child.root() != nullptr);
			this_log() << "starting child process: " << child << std::endl;
			auto result = _apps.emplace(process_id, std::move(child));
			poller().emplace(
				sys::poll_event{parent_in, sys::poll_event::In, 0},
				handler_type(&result.first->second)
			);
			poller().emplace(
				sys::poll_event{parent_out, 0, 0},
				handler_type(&result.first->second)
			);
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
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
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
			return !(this->is_stopped() and _apps.empty());
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
		on_process_exit(sys::process& p, sys::proc_info status) {
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
		sys::process_group _procs;
		router_type _router;
	};

	template<class T, class Router>
	struct Process_child_server: public Proxy_server<T,Process_rserver<T,Router>> {

		typedef Process_rserver<T,Router> rserver_type;
		typedef Proxy_server<T,rserver_type> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;
		using typename base_server::server_type;
		using typename base_server::handler_type;
		typedef typename rserver_type::router_type router_type;

		using base_server::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type, rserver_type> map_type;
		typedef stdx::log<Process_child_server> this_log;

		Process_child_server():
		_parent(sys::pipe{Shared_fildes::In, Shared_fildes::Out}, _router)
		{}

		Process_child_server(Process_child_server&& rhs) noexcept:
		base_server(std::move(rhs))
		{}

		virtual ~Process_child_server() = default;

		void
		remove_server(server_type* ptr) override {
			this_log() << "Stopping server because parent died" << std::endl;
			if (!this->is_stopped()) {
				this->stop();
				// this->factory()->stop();
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
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
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
			// _parent.setparent(this);
			std::cout << "HELLO WORLD " << sys::poll_event(Shared_fildes::Out, 0, 0)  << std::endl;
			this_log() << "DEBUG=" << sys::poll_event(Shared_fildes::Out, 0, 0) << std::endl;
			poller().emplace(sys::poll_event{Shared_fildes::In, sys::poll_event::In, 0}, handler_type(&_parent));
			poller().emplace(sys::poll_event(Shared_fildes::Out, 0, 0), handler_type(&_parent));
		}

		void
		process_kernel(kernel_type* k) {
			_parent.send(k);
		}

		router_type _router;
		rserver_type _parent;
	};

}

namespace stdx {

	template<class T, class Router>
	struct type_traits<factory::Process_rserver<T,Router>> {
		static constexpr const char*
		short_name() { return "process_rserver"; }
		typedef factory::server_category category;
	};

	template<class T, class Router>
	struct type_traits<factory::Process_iserver<T,Router>> {
		static constexpr const char*
		short_name() { return "process_iserver"; }
		typedef factory::server_category category;
	};

	template<class T, class Router>
	struct type_traits<factory::Process_child_server<T,Router>> {
		static constexpr const char*
		short_name() { return "process_child_server"; }
		typedef factory::server_category category;
	};

}

#endif // FACTORY_PROCESS_SERVER_HH
