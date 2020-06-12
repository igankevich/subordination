#include <sstream>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>

#include <subordination/core/application.hh>
#include <subordination/core/error.hh>
#include <subordination/daemon/process_pipeline.hh>

void sbnd::process_pipeline::loop() {
    std::thread waiting_thread([this] () { wait_loop(); });
    basic_socket_pipeline::loop();
    #if defined(SBN_DEBUG)
    log("waiting for all processes to finish: pid=_", sys::this_process::id());
    #endif
    try {
        this->_procs.send(sys::signal::terminate);
    } catch (const std::system_error& err) {
        if (std::errc(err.code().value()) != std::errc::no_such_process) {
            log_error(err);
        }
    }
    if (waiting_thread.joinable()) { waiting_thread.join(); }
    lock_type lock(this->_mutex);
    wait_for_processes(lock);
}

typename sbnd::process_pipeline::app_iterator
sbnd::process_pipeline::do_add(const sbn::application& app) {
    // disallow running as superuser/supergroup
    if (!this->_allowroot) {
        if (app.user() == sys::superuser() || app.group() == sys::supergroup()) {
            sbn::throw_error("executing as superuser/supergroup is disallowed");
        }
    }
    sys::two_way_pipe data_pipe;
    const auto& p = _procs.emplace(
        [&app,this,&data_pipe] () {
            try {
                data_pipe.close_in_child();
                data_pipe.validate();
                data_pipe.child_in().unsetf(sys::fd_flag::fd_close_on_exec);
                data_pipe.child_out().unsetf(sys::fd_flag::fd_close_on_exec);
                return app.execute(data_pipe);
            } catch (const std::exception& err) {
                this->log("failed to execute _: _", app.filename(), err.what());
                // make address sanitizer happy
                #if defined(__SANITIZE_ADDRESS__)
                sys::this_process::execute_command("false");
                return 1;
                #else
                return 1;
                #endif
            } catch (...) {
                this->log("failed to execute _: _", app.filename(), "<unknown error>");
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
    auto child = std::make_shared<connection_type>(p.id(), std::move(data_pipe), app);
    child->parent(this);
    child->types(types());
    child->name(this->_name);
    log("executing app=_,credentials=_:_,pid=_", app.id(), app.user(), app.group(), p.id());
    auto result = this->_applications.emplace(app.id(), child);
    child->add(child);
    return result.first;
}

void
sbnd::process_pipeline::forward(sbn::foreign_kernel* fk) {
    #if defined(SBN_DEBUG)
    log("forward src _ dst _ src-app _ dst-app _", fk->source(), fk->destination(),
        fk->source_application_id(), fk->target_application_id());
    #endif
    lock_type lock(this->_mutex);
    auto result = find_by_app_id(fk->target_application_id());
    if (result == this->_applications.end()) {
        const auto* a = fk->target_application();
        if (!a && fk->target_application_id() == fk->source_application_id()) {
            a = fk->source_application();
        }
        if (a) {
            #if defined(SBN_DEBUG)
            log("fwd: add app _ ", *a);
            #endif
            result = this->do_add(*a);
        } else {
            sbn::throw_error("bad application id ", fk->target_application_id());
        }
    }
    result->second->forward(fk);
    poller().notify_one();
}

void sbnd::process_pipeline::process_kernels() {
    while (!this->_kernels.empty()) {
        process_kernel(this->_kernels.front());
        this->_kernels.pop();
    }
}

void sbnd::process_pipeline::process_kernel(sbn::kernel* k) {
    if (k->moves_everywhere()) {
        for (auto& pair : this->_applications) { pair.second->send(k); }
    } else {
        auto result = find_by_app_id(k->target_application_id());
        if (result == this->_applications.end()) {
            sbn::throw_error("bad application id ", k->source_application_id());
        }
        result->second->send(k);
    }
}

void sbnd::process_pipeline::wait_loop() {
    using std::this_thread::sleep_for;
    using std::chrono::milliseconds;
    lock_type lock(this->_mutex);
    while (!stopping()) {
        if (this->_procs.size() == 0) {
            sys::unlock_guard<lock_type> g(lock);
            sleep_for(milliseconds(99));
        } else {
            wait_for_processes(lock);
        }
    }
}

void sbnd::process_pipeline::wait_for_processes(lock_type& lock) {
    try {
        this->_procs.wait(lock,
            [this] (const sys::process& p, sys::process_status status) {
                auto result = this->find_by_process_id(p.id());
                if (result != this->_applications.end()) {
                    this->log("app exited: app=_,_", result->first, status);
                    //result->second->close();
                    this->_applications.erase(result);
                }
            }
        );
    } catch (const std::system_error& err) {
        if (std::errc(err.code().value()) != std::errc::no_child_process) {
            log_error(err);
        }
    }
}

typename sbnd::process_pipeline::app_iterator
sbnd::process_pipeline::find_by_process_id(sys::pid_type pid) {
    using value_type = typename application_table::value_type;
    return std::find_if(
        this->_applications.begin(),
        this->_applications.end(),
        [pid] (const value_type& rhs) {
            return rhs.second->child_process_id() == pid;
        }
    );
}
