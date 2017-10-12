#include "process_pipeline.hh"

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>
#include <unistdx/it/queue_popper>

#include <bscheduler/config.hh>
#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/application.hh>
#include <bscheduler/ppl/basic_router.hh>
#include <bscheduler/ppl/kernel_protocol.hh>

namespace bsc {

	template <class K, class R>
	class process_notify_handler: public basic_handler {

	public:
		typedef K kernel_type;
		typedef process_pipeline<K,R> this_type;
		typedef typename this_type::queue_popper queue_popper;

	private:
		this_type& _ppl;

	public:

		explicit
		process_notify_handler(this_type& ppl):
		_ppl(ppl) {}

		void
		handle(const sys::epoll_event& ev) override {
			std::for_each(
				queue_popper(this->_kernels),
				queue_popper(),
				[this] (kernel_type* rhs) {
					this->_ppl.process_kernel(rhs);
				}
			);
		}

	};

	template<class K, class R>
	class process_handler: public basic_handler {

	public:
		typedef K kernel_type;
		typedef R router_type;

	private:
		typedef basic_kernelbuf<sys::fildesbuf> kernelbuf_type;
		typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
		typedef kstream<K> stream_type;
		typedef kernel_protocol<K,R,bits::forward_to_parent<R>>
			protocol_type;

	private:
		sys::pid_type _childpid;
		kernelbuf_ptr _outbuf;
		stream_type _ostream;
		kernelbuf_ptr _inbuf;
		stream_type _istream;
		protocol_type _proto;
		application _application;

	public:

		/// Called from parent process.
		process_handler(
			sys::pid_type&& child,
			sys::two_way_pipe&& pipe,
			const application& app
		):
		_childpid(child),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto(),
		_application(app)
		{
			this->_proto.set_other_application(&this->_application);
			this->_outbuf->setfd(std::move(pipe.parent_out()));
			this->_outbuf->fd().validate();
			this->_inbuf->setfd(std::move(pipe.parent_in()));
			this->_inbuf->fd().validate();
			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
		}

		process_handler(process_handler&& rhs):
		_childpid(rhs._childpid),
		_outbuf(std::move(rhs._outbuf)),
		_ostream(std::move(rhs._ostream)),
		_inbuf(std::move(rhs._inbuf)),
		_istream(std::move(rhs._istream)),
		_proto(std::move(rhs._proto)),
		_application(std::move(rhs._application))
		{
			this->_inbuf->fd().validate();
			this->_outbuf->fd().validate();
			this->_istream.rdbuf(this->_inbuf.get());
			this->_ostream.rdbuf(this->_outbuf.get());
		}

		/// Called from child process.
		explicit
		process_handler(sys::pipe&& pipe):
		_childpid(sys::this_process::id()),
		_outbuf(new kernelbuf_type),
		_ostream(_outbuf.get()),
		_inbuf(new kernelbuf_type),
		_istream(_inbuf.get()),
		_proto(),
		_application()
		{
			this->_outbuf->setfd(std::move(pipe.out()));
			this->_inbuf->setfd(std::move(pipe.in()));
			this->_proto.setf(kernel_proto_flag::prepend_source_and_destination);
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

		const application&
		app() const noexcept {
			return this->_application;
		}

		void
		close() {
			this->_outbuf->fd().close();
			this->_inbuf->fd().close();
		}

		void
		send(kernel_type* k) {
			this->_proto.send(k, this->_ostream);
		}

		void
		handle(const sys::epoll_event& event) override {
			if (this->is_starting()) {
				this->setstate(pipeline_state::started);
			}
			this->log("_ _", __func__, event);
			if (event.fd() == this->_outbuf->fd()) {
				this->_ostream.sync();
			} else {
				this->_istream.sync();
				this->_proto.receive_kernels(this->_istream);
			}
		}

		void
		forward(kernel_header& hdr, sys::pstream& istr) {
			// remove application before forwarding
			// to child process
			hdr.aptr(nullptr);
			this->_proto.forward(hdr, istr, this->_ostream);
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->pipeline_base::set_name(rhs);
			this->_proto.set_name(rhs);
			#ifndef NDEBUG
			if (this->_inbuf) {
				this->_inbuf->set_name(rhs);
			}
			if (this->_outbuf) {
				this->_outbuf->set_name(rhs);
			}
			#endif
		}

	};


}

template <class K, class R>
void
bsc::process_pipeline<K,R>
::do_run() {
	this->emplace_notify_handler(std::make_shared<process_notify_handler>(*this));
	std::thread waiting_thread {
		&process_pipeline::wait_for_all_processes_to_finish,
		this
	};
	base_pipeline::do_run();
	#ifndef NDEBUG
	this->log(
		"waiting for all processes to finish: pid=_",
		sys::this_process::id()
	);
	#endif
	if (waiting_thread.joinable()) {
		waiting_thread.join();
	}
}

template <class K, class R>
typename bsc::process_pipeline<K,R>::app_iterator
bsc::process_pipeline<K,R>
::do_add(const application& app) {
	app.allow_root(this->_allowroot);
	sys::two_way_pipe data_pipe;
	const sys::process& p = _procs.emplace(
		[&app,this,&data_pipe] () {
		    data_pipe.close_in_child();
		    data_pipe.validate();
		    try {
		        return app.execute(data_pipe);
			} catch (const std::exception& err) {
		        this->log(
					"failed to execute _: _",
					app.filename(),
					err.what()
		        );
		        // make address sanitizer happy
				#if defined(__SANITIZE_ADDRESS__)
		        return sys::this_process::execute_command("false");
				#else
		        return 1;
				#endif
			} catch (...) {
		        this->log(
					"failed to execute _: _",
					app.filename(),
					"<unknown error>"
		        );
		        // make address sanitizer happy
				#if defined(__SANITIZE_ADDRESS__)
		        return sys::this_process::execute_command("false");
				#else
		        return 1;
				#endif
			}
		}
	                        );
	data_pipe.close_in_parent();
	data_pipe.validate();
	sys::fd_type parent_in = data_pipe.parent_in().get_fd();
	sys::fd_type parent_out = data_pipe.parent_out().get_fd();
	auto child =
		std::make_shared<process_handler>(
			p.id(),
			std::move(data_pipe),
			app
		);
	child->set_name(this->_name);
	this->log(
		"executing app=_,credentials=_:_,role=_,pid=_",
		app.id(),
		app.uid(),
		app.gid(),
		app.role(),
		p.id()
	);
	auto result = this->_apps.emplace(app.id(), child);
	this->emplace_handler(sys::epoll_event(parent_in, sys::event::in), child);
	this->emplace_handler(sys::epoll_event(parent_out, sys::event::out), child);
	return result.first;
}

template <class K, class R>
void
bsc::process_pipeline<K,R>
::forward(kernel_header& hdr, sys::pstream& istr) {
	// do not lock here as static_lock locks both mutexes
	assert(this->other_mutex());
	app_iterator result = this->find_by_app_id(hdr.app());
	if (result == this->_apps.end()) {
		if (const application* a = hdr.aptr()) {
			a->make_slave();
			this->log("fwd: add app _ ", *a);
			result = this->do_add(*a);
		} else {
			BSCHEDULER_THROW(error, "bad application id");
		}
	}
	this->log("fwd _ to _", hdr, hdr.app());
	result->second->forward(hdr, istr);
}

template <class K, class R>
void
bsc::process_pipeline<K,R>
::process_kernel(kernel_type* k) {
	typedef typename map_type::value_type value_type;
	if (k->moves_everywhere()) {
		std::for_each(
			this->_apps.begin(),
			this->_apps.end(),
			[k] (value_type& rhs) {
			    rhs.second->send(k);
			}
		);
	} else {
		app_iterator result = this->find_by_app_id(k->app());
		if (result == this->_apps.end()) {
			BSCHEDULER_THROW(error, "bad application id");
		}
		result->second->send(k);
	}
}

template <class K, class R>
void
bsc::process_pipeline<K,R>
::wait_for_all_processes_to_finish() {
	using std::this_thread::sleep_for;
	using std::chrono::milliseconds;
	lock_type lock(this->_mutex);
	while (!this->has_stopped()) {
		if (this->_procs.size() == 0) {
			sys::unlock_guard<lock_type> g(lock);
			sleep_for(milliseconds(777));
		} else {
			try {
				using namespace std::placeholders;
				this->_procs.wait(
					lock,
					std::bind(&process_pipeline::on_process_exit, this, _1, _2)
				);
			} catch (const std::system_error& err) {
				if (std::errc(err.code().value()) !=
				    std::errc::no_child_process) {
					this->log_error(err);
				}
			}
		}
	}
}

template <class K, class R>
void
bsc::process_pipeline<K,R>
::on_process_exit(const sys::process& p, sys::proc_info status) {
	lock_type lock(this->_mutex);
	app_iterator result = this->find_by_process_id(p.id());
	if (result != this->_apps.end()) {
		this->log("app exited: app=_,_", result->first, status);
		result->second->close();
		this->_apps.erase(result);
	}
}

template <class K, class R>
typename bsc::process_pipeline<K,R>::app_iterator
bsc::process_pipeline<K,R>
::find_by_process_id(sys::pid_type pid) {
	typedef typename map_type::value_type value_type;
	return std::find_if(
		this->_apps.begin(),
		this->_apps.end(),
		[pid] (const value_type& rhs) {
		    return rhs.second->childpid() == pid;
		}
	);
}

template class bsc::process_pipeline<
		BSCHEDULER_KERNEL_TYPE,
		bsc::basic_router<BSCHEDULER_KERNEL_TYPE>>;
