#include <sstream>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>

#include <subordination/core/application.hh>
#include <subordination/core/error.hh>
#include <subordination/core/process_pipeline.hh>

void sbn::process_pipeline::loop() {
    std::thread waiting_thread([this] () { this->wait_loop(); });
    basic_socket_pipeline::loop();
    #if defined(SBN_DEBUG)
    this->log("waiting for all processes to finish: pid=_", sys::this_process::id());
    #endif
    if (waiting_thread.joinable()) { waiting_thread.join(); }
}

typename sbn::process_pipeline::app_iterator
sbn::process_pipeline::do_add(const application& app) {
    // disallow running as superuser/supergroup
    if (!this->_allowroot) {
        if (app.uid() == sys::superuser() || app.gid() == sys::supergroup()) {
            throw std::runtime_error("executing as superuser/supergroup is disallowed");
        }
    }
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
    auto child = std::make_shared<event_handler_type>(p.id(), std::move(data_pipe), app);
    child->parent(this);
    child->name(this->_name);
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

void
sbn::process_pipeline
::forward(foreign_kernel* hdr) {
    #if defined(SBN_DEBUG)
    this->log("forward _", hdr->header());
    #endif
    // do not lock here as static_lock locks both mutexes
    assert(this->other_mutex());
    app_iterator result = this->find_by_app_id(hdr->application_id());
    if (result == this->_apps.end()) {
        if (const application* a = hdr->application()) {
            a->make_slave();
            #if defined(SBN_DEBUG)
            this->log("fwd: add app _ ", *a);
            #endif
            result = this->do_add(*a);
        } else {
            SUBORDINATION_THROW(error, "bad application id");
        }
    }
    //#if defined(SBN_DEBUG)
    //this->log("fwd _ to _", *hdr, hdr->app());
    //#endif
    result->second->forward(hdr);
    this->poller().notify_one();
}

void sbn::process_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        this->process_kernel(this->_kernels.front());
        this->_kernels.pop();
    }
}

void sbn::process_pipeline::process_kernel(kernel* k) {
    typedef typename application_table::value_type value_type;
    if (k->moves_everywhere()) {
        std::for_each(
            this->_apps.begin(),
            this->_apps.end(),
            [k] (value_type& rhs) {
                rhs.second->send(k);
            }
        );
    } else {
        app_iterator result = this->find_by_app_id(k->application_id());
        if (result == this->_apps.end()) {
            SUBORDINATION_THROW(error, "bad application id");
        }
        result->second->send(k);
    }
}

void sbn::process_pipeline::wait_loop() {
    using std::this_thread::sleep_for;
    using std::chrono::milliseconds;
    lock_type lock(this->_mutex);
    while (!this->stopped()) {
        if (this->_procs.size() == 0) {
            sys::unlock_guard<lock_type> g(lock);
            sleep_for(milliseconds(777));
        } else {
            try {
                this->_procs.wait(lock,
                    [this] (const sys::process& p, sys::process_status status) {
                        auto result = this->find_by_process_id(p.id());
                        if (result != this->_apps.end()) {
                            this->log("app exited: app=_,_", result->first, status);
                            //result->second->close();
                            this->_apps.erase(result);
                        }
                    }
                );
            } catch (const std::system_error& err) {
                if (std::errc(err.code().value()) != std::errc::no_child_process) {
                    this->log_error(err);
                }
            }
        }
    }
}

typename sbn::process_pipeline::app_iterator
sbn::process_pipeline
::find_by_process_id(sys::pid_type pid) {
    typedef typename application_table::value_type value_type;
    return std::find_if(
        this->_apps.begin(),
        this->_apps.end(),
        [pid] (const value_type& rhs) {
            return rhs.second->child_process_id() == pid;
        }
    );
}
