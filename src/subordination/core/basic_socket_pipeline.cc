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
        using namespace std::chrono;
        log("wait for _", duration_cast<milliseconds>(dt).count());
        auto t0 = clock_type::now();
        poller().wait_for(lock, dt);
        auto t1 = clock_type::now();
        log("waited for _", duration_cast<milliseconds>(t1-t0).count());
        process_kernels();
        handle_events();
        flush_buffers();
    }
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
            if (conn->state() == connection_state::started) {
                log("started _", conn->socket_address());
            }
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
                tmp->activate(tmp);
                tmp->flush();
                for (const auto& conn : this->_connections) {
                    if (conn) {
                        log("status _ _", conn->socket_address(), conn->state());
                    }
                }
            }
        } else {
            try {
                log("flush _ _", conn->socket_address(), conn->state());
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
    this->_semaphore.notify_all();
}

void sbn::basic_socket_pipeline::wait() {
    auto& t = this->_thread;
    if (t.joinable()) { t.join(); }
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopped);
}

void sbn::basic_socket_pipeline::clear() {
    std::vector<std::unique_ptr<kernel>> sack;
    while (!this->_kernels.empty()) {
        this->_kernels.front()->mark_as_deleted(sack);
        this->_kernels.pop();
    }
    for (auto& conn : this->_connections) { if (conn) { conn->clear(); } }
}
