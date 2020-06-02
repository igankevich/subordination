#include <sstream>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>

#include <subordination/kernel/kstream.hh>
#include <subordination/ppl/application.hh>
#include <subordination/ppl/basic_router.hh>
#include <subordination/ppl/kernel_protocol.hh>
#include <subordination/ppl/process_pipeline.hh>

template <class K, class R>
void
sbn::process_pipeline<K,R>
::do_run() {
//	this->emplace_notify_handler(
//		std::make_shared<process_notify_handler<K,R>>(*this)
//	);
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
typename sbn::process_pipeline<K,R>::app_iterator
sbn::process_pipeline<K,R>
::do_add(const application& app) {
    app.allow_root(this->_allowroot);
    sys::two_way_pipe data_pipe;
    const sys::process& p = _procs.emplace(
        [&app,this,&data_pipe] () {
            try {
                data_pipe.close_in_child();
                data_pipe.validate();
                data_pipe.child_in().unsetf(sys::fd_flag::fd_close_on_exec);
                data_pipe.child_out().unsetf(sys::fd_flag::fd_close_on_exec);
                return app.execute(data_pipe);
            } catch (const std::exception& err) {
                this->log(
                    "failed to execute _: _",
                    app.filename(),
                    err.what()
                );
                // make address sanitizer happy
                #if defined(__SANITIZE_ADDRESS__)
                sys::this_process::execute_command("false");
                return 1;
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
                sys::this_process::execute_command("false");
                return 1;
                #else
                return 1;
                #endif
            }
        }
                            );
    data_pipe.close_in_parent();
    data_pipe.validate();
    sys::fd_type parent_in = data_pipe.parent_in().fd();
    sys::fd_type parent_out = data_pipe.parent_out().fd();
    auto child =
        std::make_shared<event_handler_type>(
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
sbn::process_pipeline<K,R>
::forward(foreign_kernel* hdr) {
    #ifndef NDEBUG
    this->log("forward _", hdr->header());
    #endif
    // do not lock here as static_lock locks both mutexes
    assert(this->other_mutex());
    app_iterator result = this->find_by_app_id(hdr->app());
    if (result == this->_apps.end()) {
        if (const application* a = hdr->aptr()) {
            a->make_slave();
            #ifndef NDEBUG
            this->log("fwd: add app _ ", *a);
            #endif
            result = this->do_add(*a);
        } else {
            SUBORDINATION_THROW(error, "bad application id");
        }
    }
    //#ifndef NDEBUG
    //this->log("fwd _ to _", *hdr, hdr->app());
    //#endif
    result->second->forward(hdr);
    this->poller().notify_one();
}

template <class K, class R>
void
sbn::process_pipeline<K,R>
::process_kernels() {
    std::for_each(
        queue_popper(this->_kernels),
        queue_popper(),
        [this] (kernel_type* rhs) {
            this->process_kernel(rhs);
        }
    );
}

template <class K, class R>
void
sbn::process_pipeline<K,R>
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
            SUBORDINATION_THROW(error, "bad application id");
        }
        result->second->send(k);
    }
}

template <class K, class R>
void
sbn::process_pipeline<K,R>
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
sbn::process_pipeline<K,R>
::on_process_exit(const sys::process& p, sys::process_status status) {
    lock_type lock(this->_mutex);
    app_iterator result = this->find_by_process_id(p.id());
    if (result != this->_apps.end()) {
        this->log("app exited: app=_,_", result->first, status);
//		result->second->close();
        this->_apps.erase(result);
    }
}

template <class K, class R>
typename sbn::process_pipeline<K,R>::app_iterator
sbn::process_pipeline<K,R>
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

template <class K, class R>
void
sbn::process_pipeline<K,R>
::print_state(std::ostream& out) {
    typedef typename map_type::value_type value_type;
    lock_type lock(this->_mutex);
    for (const value_type& val : this->_apps) {
        this->log("app _, handler _", val.first, *val.second);
    }
    for (const auto& p : this->_procs) {
        this->log("process _", p.id());
    }
}

template class sbn::process_pipeline<sbn::kernel, sbn::basic_router<sbn::kernel>>;
