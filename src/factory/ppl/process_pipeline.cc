#include "process_pipeline.hh"

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>
#include <unistdx/it/queue_popper>

#include <factory/config.hh>
#include <factory/ppl/basic_router.hh>

template <class K, class R>
void
factory::process_pipeline<K,R>
::remove_client(event_handler_ptr ptr) {
	this->_apps.erase(ptr->childpid());
}

template <class K, class R>
void
factory::process_pipeline<K,R>
::process_kernels() {
	lock_type lock(this->_mutex);
	std::for_each(
		queue_popper(this->_kernels),
		queue_popper(),
		[this] (kernel_type* rhs) { this->process_kernel(rhs); }
	);
}

template <class K, class R>
void
factory::process_pipeline<K,R>
::do_run() {
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
void
factory::process_pipeline<K,R>
::add(const Application& app) {
	lock_type lock(this->_mutex);
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
			}
		}
	                        );
	data_pipe.close_in_parent();
	data_pipe.validate();
	sys::fd_type parent_in = data_pipe.parent_in().get_fd();
	sys::fd_type parent_out = data_pipe.parent_out().get_fd();
//	#ifndef NDEBUG
//	this->log("exec _,pid=_,in=_,out_", app, p.id(), parent_in, parent_out);
//	#endif
	std::clog << "123=" << 123 << std::endl;
	event_handler_ptr child =
		std::make_shared<event_handler_type>(
			p.id(),
			std::move(data_pipe)
		);
	child->set_name(this->_name);
	// child.setparent(this);
	// assert(child.root() != nullptr);
	auto result = this->_apps.emplace(app.id(), child);
	this->poller().enqueue_emplace(
		sys::poll_event {parent_in, sys::poll_event::In, 0},
		result.first->second
	);
	this->poller().enqueue_emplace(
		sys::poll_event {parent_out, 0, 0},
		result.first->second
	);
	this->poller().notify_one();
}

template <class K, class R>
void
factory::process_pipeline<K,R>
::forward(const kernel_header& hdr, sys::pstream& istr) {
	this->log("fwd _", hdr);
	app_iterator result = this->find_by_app_id(hdr.app());
	if (result == this->_apps.end()) {
		FACTORY_THROW(Error, "bad application id");
	}
	result->second->forward(hdr, istr);
}

template <class K, class R>
void
factory::process_pipeline<K,R>
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
			FACTORY_THROW(Error, "bad application id");
		}
		result->second->send(k);
	}
}

template <class K, class R>
void
factory::process_pipeline<K,R>
::wait_for_all_processes_to_finish() {
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

template <class K, class R>
void
factory::process_pipeline<K,R>
::on_process_exit(const sys::process& p, sys::proc_info status) {
	this->log("process exited: _", status);
	lock_type lock(this->_mutex);
	app_iterator result = this->find_by_process_id(p.id());
	if (result != this->_apps.end()) {
		#ifndef NDEBUG
		this->log("app finished: app=_", result->first);
		#endif
		result->second->close();
	}
}

template <class K, class R>
typename factory::process_pipeline<K,R>::app_iterator
factory::process_pipeline<K,R>
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

template class factory::process_pipeline<
		FACTORY_KERNEL_TYPE,
		factory::basic_router<FACTORY_KERNEL_TYPE>>;
