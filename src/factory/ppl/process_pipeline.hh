#ifndef FACTORY_PPL_PROCESS_SERVER_HH
#define FACTORY_PPL_PROCESS_SERVER_HH

#include <map>
#include <cassert>
#include <atomic>

#include <unistdx/base/delete_each>
#include <unistdx/base/log_message>
#include <unistdx/base/unlock_guard>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/pipe>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/it/queue_popper>
#include <unistdx/net/pstream>

#include <factory/ppl/basic_pipeline.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/application.hh>
#include <factory/kernel/kernel_stream.hh>
#include <factory/kernel/kernel.hh>

namespace factory {

	template<class T, class Router, class Kernels=std::deque<T*>,
		class Traits=sys::deque_traits<Kernels>>
	struct Buffered_pipeline: public pipeline_base {

		typedef pipeline_base base_pipeline;
		typedef T kernel_type;
		typedef Router router_type;
		typedef Kernels pool_type;
		typedef Traits traits_type;
		typedef sys::queue_pop_iterator<pool_type,traits_type> queue_popper;

		explicit
		Buffered_pipeline(router_type& router):
		_router(router)
		{}

		Buffered_pipeline(Buffered_pipeline&& rhs):
		base_pipeline(std::move(rhs)),
		_buffer(std::move(rhs._buffer)),
		_router(rhs._router)
		{}

		Buffered_pipeline(const Buffered_pipeline&) = delete;
		Buffered_pipeline& operator=(const Buffered_pipeline&) = delete;

		virtual
		~Buffered_pipeline() {
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
				std::bind(&Buffered_pipeline::recover_kernel, this, _1)
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
	struct process_handler: public Buffered_pipeline<T,Router> {

		typedef Buffered_pipeline<T,Router> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::router_type;
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
		typedef sys::pstream stream_type;
		typedef typename kernel_type::app_type app_type;

		using base_pipeline::_router;

		process_handler(
			sys::pid_type&& child,
			sys::two_way_pipe&& pipe,
			router_type& router
		):
		base_pipeline(router),
		_childpid(child),
		_outbuf(std::move(pipe.parent_out())),
		_ostream(&_outbuf),
		_inbuf(std::move(pipe.parent_in())),
		_istream(&_inbuf)
		{
			this->_inbuf.fd().validate();
			this->_outbuf.fd().validate();
		}

		process_handler(process_handler&& rhs):
		base_pipeline(std::move(rhs)),
		_childpid(rhs._childpid),
		_outbuf(std::move(rhs._outbuf)),
		_ostream(&_outbuf),
		_inbuf(std::move(rhs._inbuf)),
		_istream(&_inbuf)
		{
			this->_inbuf.fd().validate();
			this->_outbuf.fd().validate();
		}

		explicit
		process_handler(sys::pipe&& pipe, router_type& router):
		base_pipeline(router),
		_childpid(sys::this_process::id()),
		_outbuf(std::move(pipe.out())),
		_ostream(&_outbuf),
		_inbuf(std::move(pipe.in())),
		_istream(&_inbuf)
		{}

		const sys::pid_type&
		childpid() const {
			return this->_childpid;
		}

		void
		close() {
			this->_outbuf.fd().close();
			this->_inbuf.fd().close();
		}

		void
		send(kernel_type* kernel) {
			/*
			TODO we need IDs only when forwarding kernels to other hosts
			if (!kernel->has_id() && !kernel->moves_everywhere()) {
				kernel->id(this->factory()->factory_generate_id());
			}
			*/
			this->_ostream.begin_packet();
			this->_ostream << kernel->app() << kernel->from();
			Types::const_iterator type = types.find(typeid(*kernel));
			if (type == types.end()) {
				throw Kernel_error("no type is defined for the kernel", kernel->id());
			}
			this->_ostream << type->id();
			kernel->write(this->_ostream);
			this->_ostream.end_packet();
			#ifndef NDEBUG
			sys::log_message("app", "send to _ kernel _", this->_childpid, *kernel);
			#endif
			base_pipeline::send(kernel);
		}

		void
		handle(sys::poll_event& event) {
			if (event.fd() == this->_outbuf.fd()) {
//				this->_ostream.clear();
				this->_ostream.sync();
			} else {
				assert(
					event.fd() == _inbuf.fd()
					or !_inbuf.fd()
					or !event.bad_fd()
				);
				assert(!event.out() || event.hup());
//				this->_istream.clear();
				this->_istream.sync();
				while (this->_istream.read_packet()) {
					read_and_receive_kernel();
				}
			}
		}

		void
		forward(const Kernel_header& hdr, sys::pstream& istr) {
			this->_ostream.begin_packet();
			this->_ostream.append_payload(istr);
			this->_ostream.end_packet();
			#ifndef NDEBUG
			sys::log_message("app", "forward _", hdr);
			#endif
		}

		friend std::ostream&
		operator<<(std::ostream& out, const process_handler& rhs) {
			return out << sys::make_object("childpid", rhs._childpid);
		}

	private:

		void read_and_receive_kernel() {
			app_type app; sys::endpoint from;
			this->_istream >> app >> from;
			#ifndef NDEBUG
			sys::log_message("app", "recv app=_,from=_", app, from);
			#endif
			if (app == Application::ROOT ||
					this->_childpid == sys::this_process::id()) {
				kernel_type* k = this->read_kernel();
				this->receive_kernel(k, app);
			} else {
				Kernel_header hdr;
				hdr.setapp(app);
				this->_router.forward(hdr, this->_istream);
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
	struct process_pipeline:
		public basic_socket_pipeline<T,process_handler<T,Router>> {

		typedef Router router_type;

		typedef basic_socket_pipeline<T,process_handler<T,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;

		using base_pipeline::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type,event_handler_ptr> map_type;

		process_pipeline() = default;

		process_pipeline(const process_pipeline&) = delete;
		process_pipeline& operator=(const process_pipeline&) = delete;

		process_pipeline(process_pipeline&& rhs) = default;

		~process_pipeline() = default;

		void
		remove_pipeline(event_handler_ptr ptr) override {
			this->_apps.erase(ptr->childpid());
		}

		void
		process_kernels() override {
			lock_type lock(this->_mutex);
			sys::for_each_unlock(lock,
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

		void
		add(const Application& app) {
			lock_type lock(this->_mutex);
			sys::two_way_pipe data_pipe;
			const sys::process& p = _procs.emplace([&app,this,&data_pipe] () {
				data_pipe.close_in_child();
				data_pipe.remap_in_child(Shared_fildes::In, Shared_fildes::Out);
				data_pipe.validate();
				return app.execute();
			});
			sys::pid_type process_id = p.id();
			#ifndef NDEBUG
			sys::log_message("app", "exec _,pid=_", app, process_id);
			#endif
			data_pipe.close_in_parent();
			data_pipe.validate();
			sys::fd_type parent_in = data_pipe.parent_in().get_fd();
			sys::fd_type parent_out = data_pipe.parent_out().get_fd();
			event_handler_ptr child(new event_handler_type(p.id(), std::move(data_pipe), _router));
			// child.setparent(this);
			// assert(child.root() != nullptr);
			auto result = this->_apps.emplace(process_id, child);
			poller().emplace(
				sys::poll_event{parent_in, sys::poll_event::In, 0},
				result.first->second
			);
			poller().emplace(sys::poll_event{parent_out, 0, 0}, result.first->second);
		}

		void
		do_run() override {
			//std::thread waiting_thread{
			//	&process_pipeline::wait_for_all_processes_to_finish,
			//	this
			//};
			base_pipeline::do_run();
			#ifndef NDEBUG
			sys::log_message(
				this->_name,
				"waiting for all processes to finish: pid=_",
				sys::this_process::id()
			);
			#endif
			//if (waiting_thread.joinable()) {
			//	waiting_thread.join();
			//}
		}

		void
		forward(const Kernel_header& hdr, sys::pstream& istr) {
			auto result = this->_apps.find(hdr.app());
			if (result == this->_apps.end()) {
				FACTORY_THROW(Error, "bad application id");
			}
			result->second->forward(hdr, istr);
		}

	private:

		void
		process_kernel(kernel_type* k) {
			typedef typename map_type::value_type value_type;
			if (k->moves_everywhere()) {
				std::for_each(this->_apps.begin(), this->_apps.end(),
					[k] (value_type& rhs) {
						rhs.second->send(k);
					}
				);
			} else {
				auto result = this->_apps.find(k->app());
				if (result == this->_apps.end()) {
					FACTORY_THROW(Error, "bad application id");
				}
				result->second->send(k);
			}
		}

		void
		wait_for_all_processes_to_finish() {
			using std::this_thread::sleep_for;
			using std::chrono::milliseconds;
			lock_type lock(this->_mutex);
			while (!this->is_stopped()) {
				sys::unlock_guard<lock_type> g(lock);
				using namespace std::placeholders;
				if (this->_procs.empty()) {
					sleep_for(milliseconds(777));
				} else {
					this->_procs.wait(
						std::bind(&process_pipeline::on_process_exit, this, _1, _2)
					);
				}
			}
		}

		void
		on_process_exit(const sys::process& p, sys::proc_info status) {
			sys::log_message(this->_name, "process exited: status=_", status);
			lock_type lock(this->_mutex);
			auto result = this->_apps.find(p.id());
			if (result != this->_apps.end()) {
				#ifndef NDEBUG
				sys::log_message(
					this->_name,
					"app finished: app=_",
					result->first
				);
				#endif
				result->second->close();
			}
		}

		map_type _apps;
		sys::process_group _procs;
		router_type _router;
	};

	template<class T, class Router>
	struct child_process_pipeline:
		public basic_socket_pipeline<T,process_handler<T,Router>> {

		typedef basic_socket_pipeline<T,process_handler<T,Router>> base_pipeline;
		using typename base_pipeline::kernel_type;
		using typename base_pipeline::mutex_type;
		using typename base_pipeline::lock_type;
		using typename base_pipeline::sem_type;
		using typename base_pipeline::kernel_pool;
		using typename base_pipeline::event_handler_type;
		using typename base_pipeline::event_handler_ptr;
		using typename base_pipeline::queue_popper;
		typedef typename event_handler_type::router_type router_type;

		using base_pipeline::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type, event_handler_type> map_type;

		child_process_pipeline():
		_parent(new event_handler_type(
			sys::pipe{Shared_fildes::In, Shared_fildes::Out},
			this->_router
		))
		{}

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual ~child_process_pipeline() = default;

		void
		remove_pipeline(event_handler_ptr ptr) override {
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
				[this] (kernel_type* rhs) { this->process_kernel(rhs); }
			);
		}

		void
		forward(const Kernel_header& hdr, sys::pstream& istr) {
			assert(false);
		}

		void
		do_run() override {
			this->init_pipeline();
			base_pipeline::do_run();
		}

	private:

		void
		init_pipeline() {
			// _parent.setparent(this);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::In, sys::poll_event::In, 0},
				this->_parent
			);
			this->poller().emplace(
				sys::poll_event{Shared_fildes::Out, 0, 0},
				this->_parent
			);
		}

		void
		process_kernel(kernel_type* k) {
			this->_parent->send(k);
		}

		router_type _router;
		event_handler_ptr _parent;
	};

}

#endif // vim:filetype=cpp
