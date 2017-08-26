#ifndef FACTORY_PROCESS_SERVER_HH
#define FACTORY_PROCESS_SERVER_HH

#include <map>
#include <cassert>
#include <atomic>

#include <unistdx/base/delete_each>
#include <unistdx/base/unlock_guard>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/pipe>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/queue_popper>
#include <unistdx/net/pstream>

#include <factory/ppl/basic_server.hh>
#include <factory/ppl/proxy_server.hh>
#include <factory/ppl/application.hh>
#include <factory/kernel/kernel_stream.hh>
#include <factory/kernel/kernel.hh>

namespace factory {

	template<class T, class Router, class Kernels=std::deque<T*>,
		class Traits=sys::deque_traits<Kernels>>
	struct Buffered_server: public Server_base {

		typedef Server_base base_server;
		typedef T kernel_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<pool_type,traits_type> queue_popper;

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
			if ((kernel->moves_upstream() || kernel->moves_somewhere()) && kernel->has_id()) {
				_buffer.push_back(kernel);
				erase_kernel = false;
			}
			if (erase_kernel && !kernel->moves_everywhere()) {
				delete kernel;
			}
		}

	protected:

		void
		clear_kernel_buffer(kernel_type* k) {
			auto pos = std::find_if(_buffer.begin(), _buffer.end(),
				[k] (kernel_type* rhs) { return *rhs == *k; });
			if (pos != _buffer.end()) {
				kernel_type* orig = *pos;
				k->parent(orig->parent());
				k->principal(k->parent());
				delete orig;
				_buffer.erase(pos);
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

			// recover kernels written to output buffer
			using namespace std::placeholders;
			std::for_each(
				queue_popper(_buffer),
				queue_popper(),
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
			sys::delete_each(queue_popper(_buffer), queue_popper());
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
		typedef sys::pstream stream_type;
		typedef typename kernel_type::app_type app_type;

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
		close() {
			_outbuf.fd().close();
			_inbuf.fd().close();
		}

		void
		send(kernel_type* kernel) {
			/*
			TODO we need IDs only when forwarding kernels to other hosts
			if (!kernel->has_id() && !kernel->moves_everywhere()) {
				kernel->id(this->factory()->factory_generate_id());
			}
			*/
			_ostream.begin_packet();
			_ostream << kernel->app() << kernel->from();
			Types::const_iterator type = types.find(typeid(*kernel));
			if (type == types.end()) {
				throw Kernel_error("no type is defined for the kernel", kernel->id());
			}
			_ostream << type->id();
			kernel->write(_ostream);
			_ostream.end_packet();
			#ifndef NDEBUG
			sys::log_message("app", "send to _ kernel _", _childpid, *kernel);
			#endif
			base_server::send(kernel);
		}

		void
		handle(sys::poll_event& event) {
			if (event.fd() == _outbuf.fd()) {
//				_ostream.clear();
				_ostream.sync();
			} else {
				assert(
					event.fd() == _inbuf.fd()
					or !_inbuf.fd()
					or !event.bad_fd()
				);
				assert(!event.out() || event.hup());
//				_istream.clear();
				_istream.sync();
				while (_istream.read_packet()) {
					read_and_receive_kernel();
				}
			}
		}

		void
		forward(const Kernel_header& hdr, sys::pstream& istr) {
			_ostream.begin_packet();
			_ostream.append_payload(istr);
			_ostream.end_packet();
			#ifndef NDEBUG
			sys::log_message("app", "forward _", hdr);
			#endif
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Process_rserver& rhs) {
			return out << sys::make_object("childpid", rhs._childpid);
		}

	private:

		void read_and_receive_kernel() {
			app_type app; sys::endpoint from;
			_istream >> app >> from;
			if (app == Application::ROOT || _childpid == sys::this_process::id()) {
				kernel_type* k = this->read_kernel();
				this->receive_kernel(k, app);
			} else {
				Kernel_header hdr;
				hdr.setapp(app);
				_router.forward(hdr, _istream);
			}
		}

		inline kernel_type*
		read_kernel() {
			return static_cast<kernel_type*>(types.read_object(this->_istream));
		}

		void
		receive_kernel(kernel_type* k, app_type app) {
			k->setapp(app);
			bool ok = true;
			if (k->moves_downstream()) {
				// TODO
				this->clear_kernel_buffer(k);
			} else if (k->principal_id()) {
				instances_guard g(instances);
				auto result = instances.find(k->principal_id());
				if (result == instances.end()) {
					k->result(Result::no_principal_found);
					ok = false;
				}
				k->principal(result->second);
			}
			#ifndef NDEBUG
			sys::log_message("app", "recv _", *k);
			#endif
			if (!ok) {
				return_kernel(k);
			} else {
				_router.send_local(k);
			}
		}

		void return_kernel(kernel_type* k) {
			#ifndef NDEBUG
			sys::log_message("app", "no principal found for _", *k);
			#endif
			k->principal(k->parent());
			this->send(k);
		}

		sys::pid_type _childpid;
		kernelbuf_type _outbuf;
		stream_type _ostream;
		kernelbuf_type _inbuf;
		stream_type _istream;
		int _refcnt = 0;

	};

	enum Shared_fildes: sys::fd_type {
		In  = 100,
		Out = 101
	};

	template<class T, class Router>
	struct Process_iserver: public Proxy_server<T,Process_rserver<T,Router>> {

		typedef Router router_type;

		typedef Proxy_server<T,Process_rserver<T,Router>> base_server;
		using typename base_server::kernel_type;
		using typename base_server::mutex_type;
		using typename base_server::lock_type;
		using typename base_server::sem_type;
		using typename base_server::kernel_pool;
		using typename base_server::server_type;
		using typename base_server::server_ptr;
		using typename base_server::queue_popper;

		using base_server::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type,server_ptr> map_type;

		Process_iserver() = default;

		Process_iserver(const Process_iserver&) = delete;
		Process_iserver& operator=(const Process_iserver&) = delete;

		Process_iserver(Process_iserver&& rhs) = default;

		~Process_iserver() = default;

		void
		remove_server(server_ptr ptr) override {
			_apps.erase(ptr->childpid());
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			sys::for_each_unlock(lock,
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { process_kernel(rhs); }
			);
		}

		void
		add(const Application& app) {
			#ifndef NDEBUG
			sys::log_message("app", "exec _", app);
			#endif
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
			sys::fd_type parent_in = data_pipe.parent_in().get_fd();
			sys::fd_type parent_out = data_pipe.parent_out().get_fd();
			server_ptr child(new server_type(p.id(), std::move(data_pipe), _router));
			// child.setparent(this);
			// assert(child.root() != nullptr);
			auto result = _apps.emplace(process_id, child);
			poller().emplace(
				sys::poll_event{parent_in, sys::poll_event::In, 0},
				result.first->second
			);
			poller().emplace(sys::poll_event{parent_out, 0, 0}, result.first->second);
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
		forward(const Kernel_header& hdr, sys::pstream& istr) {
			auto result = _apps.find(hdr.app());
			if (result == _apps.end()) {
				FACTORY_THROW(Error, "bad application id");
			}
			result->second->forward(hdr, istr);
		}

	private:

		void
		process_kernel(kernel_type* k) {
			typedef typename map_type::value_type value_type;
			if (k->moves_everywhere()) {
				std::for_each(_apps.begin(), _apps.end(),
					[k] (value_type& rhs) {
						rhs.second->send(k);
					}
				);
			} else {
				auto result = _apps.find(k->app());
				if (result == _apps.end()) {
					FACTORY_THROW(Error, "bad application id");
				}
				result->second->send(k);
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
				sys::unlock_guard<lock_type> g(lock);
				using namespace std::placeholders;
				_procs.wait(std::bind(&Process_iserver::on_process_exit, this, _1, _2));
			}
		}

		void
		on_process_exit(const sys::process& p, sys::proc_info status) {
			lock_type lock(this->_mutex);
			auto result = _apps.find(p.id());
			if (result != this->_apps.end()) {
				#ifndef NDEBUG
				sys::log_message("app", "exit, status=_, app=_", status, result->first);
				#endif
				result->second->close();
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
		using typename base_server::server_ptr;
		using typename base_server::queue_popper;
		typedef typename rserver_type::router_type router_type;

		using base_server::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type, rserver_type> map_type;

		Process_child_server():
		_parent(new server_type(sys::pipe{Shared_fildes::In, Shared_fildes::Out}, _router))
		{}

		Process_child_server(Process_child_server&& rhs) = default;

		virtual ~Process_child_server() = default;

		void
		remove_server(server_ptr ptr) override {
			if (!this->is_stopped()) {
				this->stop();
				// this->factory()->stop();
			}
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			sys::for_each_unlock(lock,
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { process_kernel(rhs); }
			);
		}

		void
		forward(const Kernel_header& hdr, sys::pstream& istr) {
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
			poller().emplace(sys::poll_event{Shared_fildes::In, sys::poll_event::In, 0}, _parent);
			poller().emplace(sys::poll_event(Shared_fildes::Out, 0, 0), _parent);
		}

		void
		process_kernel(kernel_type* k) {
			_parent->send(k);
		}

		router_type _router;
		server_ptr _parent;
	};

}

#endif // FACTORY_PROCESS_SERVER_HH