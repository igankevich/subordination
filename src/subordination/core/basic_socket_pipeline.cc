#include <memory>
#include <vector>

#include <unistdx/ipc/process>

#include <subordination/core/basic_socket_pipeline.hh>

sbn::basic_socket_pipeline::basic_socket_pipeline() {
    this->_connections.emplace(poller().pipe_in(), std::make_shared<connection>());
}

void sbn::basic_socket_pipeline::loop() {
    lock_type lock(this->_mutex);
    while (!stopping()) {
        auto result = oldest_connection();
        duration dt = std::chrono::seconds(999);
        if (result != this->_connections.end()) {
            dt = std::max((*result)->start_time_point() +
                          connection_timeout() - clock_type::now(),
                          duration::zero());
        }
        poller().wait_for(lock, dt);
        process_kernels();
        process_connections();
    }
}

void sbn::basic_socket_pipeline::process_connections() {
    handle_events();
    flush_buffers();
}

void sbn::basic_socket_pipeline::handle_events() {
    for (const auto& ev : this->poller()) {
        if (!(ev.fd() < this->_connections.size())) {
            this->log("unknown fd _", ev.fd());
            continue;
        }
        auto& conn = this->_connections[ev.fd()];
        if (!conn) { continue; }
        if (conn->state() == connection_state::inactive) { continue; }
        // process event by calling event connection function
        try {
            conn->handle(ev);
            if (!ev) {
                remove(ev.fd(), conn, "bad event");
            }
        } catch (const sys::bad_call& err) {
            if (err.errc() != std::errc::connection_refused) {
                remove(ev.fd(), conn, err.what());
            } else {
                deactivate(ev.fd(), conn, clock_type::now(), err.what());
            }
        }
    }
}

void sbn::basic_socket_pipeline::deactivate(sys::fd_type fd,
                                            connection_ptr conn,
                                            time_point now,
                                            const char* reason) {
    if (conn->attempts() >= max_connection_attempts()) {
        remove(fd, conn, "max. attempts reached");
    } else {
        log("deactivate _ (_)", conn->socket_address(), reason);
        conn->deactivate(conn);
    }
}

void sbn::basic_socket_pipeline::remove(sys::fd_type fd,
                                        connection_ptr& conn,
                                        const char* reason) {
    log("remove _ (_)", conn->socket_address(), reason);
    conn->remove(conn);
    this->_connections.erase(fd);
}

void sbn::basic_socket_pipeline::flush_buffers() {
    const auto now = clock_type::now();
    // N.B. The no. of connections is not updated since there is no need
    // to process new connections, and removed connections are skipped anyway.
    const auto nconnections = this->_connections.size();
    for (size_type i=0; i<nconnections; ++i) {
        auto& conn = this->_connections[i];
        if (!conn) { continue; }
        if (conn->state() == connection_state::stopped) {
            remove(i, conn, "stopped");
        } else if (conn->state() == connection_state::starting) {
            if (conn->start_time_point() + connection_timeout() <= now) {
                deactivate(i, conn, now, "timed out");
            }
        } else if (conn->state() == connection_state::inactive) {
            if (conn->start_time_point() + connection_timeout() <= now) {
                using namespace std::chrono;
                log("activate _ _", conn->socket_address(),
                    duration_cast<milliseconds>(conn->start_time_point()-clock_type::now()).count());
                connection_ptr tmp = conn;
                this->_connections.erase(i);
                try {
                    tmp->activate(tmp);
                    tmp->flush();
                } catch (const std::exception& err) {
                    size_type j = 0;
                    for (auto& c : this->_connections) {
                        if (c.get() == tmp.get()) { break; }
                        ++j;
                    }
                    deactivate(j, tmp, now, err.what());
                }
            }
        } else {
            try {
                conn->flush();
            } catch (const std::exception& err) {
                deactivate(i, conn, now, err.what());
            }
        }
    }
}

void sbn::basic_socket_pipeline::start() {
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::starting);
    this->_thread = std::thread([this] () {
        #if defined(UNISTDX_HAVE_PRCTL)
        ::prctl(PR_SET_NAME, this->_name);
        #endif
        loop();
    });
    this->setstate(pipeline_state::started);
}

void sbn::basic_socket_pipeline::stop() {
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopping);
    for (auto& conn : this->_connections) { if (conn) { conn->stop(); } }
    this->_semaphore.notify_all();
}

void sbn::basic_socket_pipeline::wait() {
    auto& t = this->_thread;
    if (t.joinable()) { t.join(); }
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopped);
}

void sbn::basic_socket_pipeline::clear(kernel_sack& sack) {
    while (!this->_kernels.empty()) {
        this->_kernels.front()->mark_as_deleted(sack);
        this->_kernels.pop();
    }
    for (auto& conn : this->_connections) { if (conn) { conn->clear(sack); } }
    for (auto* k : this->_listeners) { k->mark_as_deleted(sack); }
    this->_listeners.clear();
    for (auto* k : this->_trash) { k->mark_as_deleted(sack); }
    this->_trash.clear();
}

void sbn::basic_socket_pipeline::remove_listener(kernel* b) {
    this->_listeners.erase(
        std::remove_if(this->_listeners.begin(), this->_listeners.end(),
                       [b] (kernel* a) { return a == b; }),
        this->_listeners.end());
}
