#ifndef FACTORY_PPL_PROCESS_PIPELINE_HH
#define FACTORY_PPL_PROCESS_PIPELINE_HH

#include <map>
#include <cassert>
#include <atomic>
#include <memory>

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

#include <factory/kernel/kernel.hh>
#include <factory/kernel/kernel_instance_registry.hh>
#include <factory/kernel/kstream.hh>
#include <factory/ppl/application.hh>
#include <factory/ppl/basic_pipeline.hh>
#include <factory/ppl/basic_socket_pipeline.hh>
#include <factory/ppl/kernel_protocol.hh>
#include <factory/ppl/application.hh>

namespace factory {

	template<class T, class Router>
	class process_handler: public pipeline_base {

	public:
		typedef T kernel_type;
		typedef Router router_type;
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<T> stream_type;
		typedef kernel_protocol<T,Router,bits::forward_to_parent<Router>>
			protocol_type;

	private:
		sys::pid_type _childpid;
		kernelbuf_ptr _outbuf;
		stream_type _ostream;
		kernelbuf_ptr _inbuf;
		stream_type _istream;
		protocol_type _proto;

	public:

		process_handler(
			sys::pid_type&& child,
			sys::two_way_pipe&& pipe
		):
		_childpid(child),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto()
		{
			this->_outbuf->setfd(std::move(pipe.parent_out()));
			this->_outbuf->fd().validate();
			this->_inbuf->setfd(std::move(pipe.parent_in()));
			this->_inbuf->fd().validate();
//			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
		}

		process_handler(process_handler&& rhs):
		_childpid(rhs._childpid),
		_outbuf(std::move(rhs._outbuf)),
		_ostream(std::move(rhs._ostream)),
		_inbuf(std::move(rhs._inbuf)),
		_istream(std::move(rhs._istream)),
		_proto(std::move(rhs._proto))
		{
			this->_inbuf->fd().validate();
			this->_outbuf->fd().validate();
			this->_istream.rdbuf(this->_inbuf.get());
			this->_ostream.rdbuf(this->_outbuf.get());
		}

		explicit
		process_handler(sys::pipe&& pipe):
		_childpid(sys::this_process::id()),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto()
		{
			this->_outbuf->setfd(std::move(pipe.out()));
			this->_inbuf->setfd(std::move(pipe.in()));
//			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
		}

		virtual
		~process_handler() {
			// recover kernels from upstream and downstream buffer
			this->_proto.recover_kernels(true);
		}

		const sys::pid_type&
		childpid() const {
			return this->_childpid;
		}

		void
		close() {
			this->_outbuf->fd().close();
			this->_inbuf->fd().close();
		}

		void
		send(kernel_type* kernel) {
			this->_proto.send(kernel, this->_ostream);
		}

		void
		handle(sys::poll_event& event) {
			if (this->is_starting()) {
				this->setstate(pipeline_state::started);
			}
			if (event.fd() == this->_outbuf->fd()) {
//				this->_ostream.clear();
				this->_ostream.sync();
			} else {
				assert(
					event.fd() == this->_inbuf->fd()
					|| !this->_inbuf->fd()
					|| !event.bad_fd()
				);
				assert(!event.out() || event.hup());
//				this->_istream.clear();
				this->_istream.sync();
				this->_proto.receive_kernels(this->_istream);
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
			this->_proto.forward(hdr, istr, this->_ostream);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const process_handler& rhs) {
			return out << sys::make_object("childpid", rhs._childpid);
		}

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
		remove_client(event_handler_ptr ptr) override {
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
			sys::log_message(this->_name, "exec _,pid=_", app, process_id);
			#endif
			data_pipe.close_in_parent();
			data_pipe.validate();
			sys::fd_type parent_in = data_pipe.parent_in().get_fd();
			sys::fd_type parent_out = data_pipe.parent_out().get_fd();
			event_handler_ptr child =
				std::make_shared<event_handler_type>(
					p.id(),
					std::move(data_pipe)
				);
			child->set_name(this->_name);
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
			std::thread waiting_thread{
				&process_pipeline::wait_for_all_processes_to_finish,
				this
			};
			base_pipeline::do_run();
			#ifndef NDEBUG
			sys::log_message(
				this->_name,
				"waiting for all processes to finish: pid=_",
				sys::this_process::id()
			);
			#endif
			if (waiting_thread.joinable()) {
				waiting_thread.join();
			}
		}

		void
		forward(const kernel_header& hdr, sys::pstream& istr) {
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
				if (this->_procs.size() == 0) {
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
		typedef Router router_type;

		using base_pipeline::poller;

		typedef sys::pid_type key_type;
		typedef std::map<key_type, event_handler_type> map_type;

		child_process_pipeline():
		_parent(std::make_shared<event_handler_type>(
			sys::pipe{Shared_fildes::In, Shared_fildes::Out}
		))
		{}

		child_process_pipeline(child_process_pipeline&& rhs) = default;

		virtual ~child_process_pipeline() = default;

		void
		remove_client(event_handler_ptr ptr) override {
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
			this->_parent->set_name(this->name());
		}

		void
		process_kernel(kernel_type* k) {
			#ifndef NDEBUG
			this->log("send _", *k);
			#endif
			this->_parent->send(k);
		}

		event_handler_ptr _parent;
	};

}

#endif // vim:filetype=cpp
