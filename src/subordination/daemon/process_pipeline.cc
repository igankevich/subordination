#include <sstream>

#include <unistdx/base/unlock_guard>
#include <unistdx/io/two_way_pipe>

#include <subordination/bits/contracts.hh>
#include <subordination/core/application.hh>
#include <subordination/core/error.hh>
#include <subordination/daemon/process_pipeline.hh>
#include <subordination/daemon/terminate_kernel.hh>

namespace {
    inline void update_buffer_size(sys::fildes& fd, size_t new_size) {
        Expects(bool(fd));
        if (new_size == std::numeric_limits<size_t>::max()) { return; }
        fd.pipe_buffer_size(new_size);
    }
}

void sbnd::process_pipeline::loop() {
    std::thread waiting_thread([this] () { wait_loop(); });
    basic_socket_pipeline::loop();
    #if defined(SBN_DEBUG)
    log("waiting for all processes to finish: pid=_", sys::this_process::id());
    #endif
    try {
        this->_child_processes.terminate();
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
    update_buffer_size(data_pipe.in(), this->_pipe_buffer_size);
    update_buffer_size(data_pipe.out(), this->_pipe_buffer_size);
    const auto& p = _child_processes.emplace(
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
    child->unix(unix());
    using f = sbn::connection_flags;
    child->setf(f::save_upstream_kernels | f::save_downstream_kernels);
    log("executing app=_,credentials=_:_,pid=_ command _",
        app.id(), app.user(), app.group(), p.id(), app.arguments().front());
    auto result = this->_jobs.emplace(app.id(), child);
    child->add(child);
    return result.first;
}

void sbnd::process_pipeline::remove(application_id_type id) {
    lock_type lock(this->_mutex);
    auto result = this->_jobs.find(id);
    if (result == this->_jobs.end()) { return; }
    terminate(result->second->child_process_id());
    { sbn::kernel_sack sack; result->second->clear(sack); }
    this->_jobs.erase(result);
}

void sbnd::process_pipeline::terminate(sys::pid_type id) {
    Expects(id > 0);
    auto first = this->_child_processes.begin(), last = this->_child_processes.end();
    while (first != last) {
        if (first->id() == id) {
            first->terminate();
            break;
        }
        ++first;
    }
}

void sbnd::process_pipeline::forward(sbn::foreign_kernel_ptr&& fk) {
    Expects(fk);
    #if defined(SBN_DEBUG)
    log("forward _", *fk);
    log("forward src _ dst _ src-app _ dst-app _ id _", fk->source(), fk->destination(),
        fk->source_application_id(), fk->target_application_id(), fk->id());
    #endif
    lock_type lock(this->_mutex);
    if (stopping() || stopped()) { return; }
    auto current_load = total_load();
    current_load += fk->weights();
    if (num_threads_used(current_load) < this->_max_threads) {
        do_forward(std::move(fk));
        poller().notify_one();
    } else {
        this->_outstanding_kernels.emplace_back(std::move(fk));
    }
}

void sbnd::process_pipeline::do_forward(sbn::foreign_kernel_ptr&& fk) {
    auto result = this->_jobs.find(fk->target_application_id());
    if (result == this->_jobs.end()) {
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
    auto& conn = result->second;
    conn->forward(std::move(fk));
}

void sbnd::process_pipeline::do_process_kernels(kernel_queue& kernels,
                                                sbn::weight_array& current_load) {
    auto first = kernels.begin(), last = kernels.end();
    while (first != last) {
        auto new_load = current_load + (*first)->weights();
        if (num_threads_used(new_load) < this->_max_threads) {
            process_kernel(std::move(*first));
            first = kernels.erase(first);
            last = kernels.end();
            current_load = new_load;
        } else {
            ++first;
        }
    }
}

void sbnd::process_pipeline::process_kernels() {
    auto current_load = total_load();
    do_process_kernels(this->_kernels, current_load);
    do_process_kernels(this->_outstanding_kernels, current_load);
}

void sbnd::process_pipeline::process_kernel(sbn::kernel_ptr&& k) {
    if (k->phase() == sbn::kernel::phases::broadcast) {
        for (auto& pair : this->_jobs) { pair.second->send(k); }
    } else {
        const auto application_id = k->target_application_id();
        auto result = this->_jobs.find(application_id);
        if (result == this->_jobs.end()) {
            auto* a = k->target_application();
            if (!a) { sbn::throw_error("bad application id ", application_id); }
            result = do_add(*a);
        }
        result->second->send(k);
    }
}

void sbnd::process_pipeline::process_connections() {
    sbn::basic_socket_pipeline::process_connections();
    // TODO we have to find a better solution since
    // after reading transaction log we may mark running
    // process that does everything locally as stale.
    /*
    if (this->_timeout == duration::zero()) { return; }
    auto first = this->_jobs.begin(), last = this->_jobs.end();
    const auto now = clock_type::now();
    while (first != last) {
        auto& conn = first->second;
        if (conn->stale(now, this->_timeout)) {
            terminate(conn->child_process_id());
            first = this->_jobs.erase(first);
        } else { ++first; }
    }
    */
}

void sbnd::process_pipeline::wait_loop() {
    using std::this_thread::sleep_for;
    using std::chrono::milliseconds;
    lock_type lock(this->_mutex);
    while (!stopping()) {
        if (this->_child_processes.size() == 0) {
            sys::unlock_guard<lock_type> g(lock);
            sleep_for(milliseconds(99));
        } else {
            wait_for_processes(lock);
        }
    }
}

void sbnd::process_pipeline::wait_for_processes(lock_type& lock) {
    try {
        this->_child_processes.wait(lock,
            [this] (const sys::process& p, sys::process_status status) {
                auto result = this->find_by_process_id(p.id());
                if (result == this->_jobs.end()) { return; }
                this->log("app exited: app=_,_", result->first, status);
                //result->second->close();
                auto application_id = result->first;
                { sbn::kernel_sack sack; result->second->clear(sack); }
                this->_jobs.erase(result);
                if (!native_pipeline()) { return; }
                for (auto* target : this->_listeners) {
                    auto k = sbn::make_pointer<process_pipeline_kernel>();
                    k->application_id(application_id);
                    k->event(process_pipeline_event::child_process_terminated);
                    k->status(status);
                    k->principal(target);
                    k->phase(sbn::kernel::phases::point_to_point);
                    send_native(std::move(k));
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
    Expects(pid > 0);
    using value_type = typename application_table::value_type;
    return std::find_if(
        this->_jobs.begin(),
        this->_jobs.end(),
        [pid] (const value_type& rhs) {
            return rhs.second->child_process_id() == pid;
        }
    );
}
