#include <memory>
#include <vector>

#include <unistdx/ipc/process>

#include <subordination/core/basic_socket_pipeline.hh>

void sbn::basic_socket_pipeline::loop() {
    static_lock_type lock(&this->_mutex, this->_othermutex);
    while (!this->stopping()) {
        bool timeout = false;
        if (this->_start_timeout > duration::zero()) {
            handler_const_iterator result =
                this->handler_with_min_start_time_point();
            if (result != this->_connections.end()) {
                timeout = true;
                const time_point tp = result->second->start_time_point()
                                      + this->_start_timeout;
                this->poller().wait_until(lock, tp);
            }
        }
        auto process = [this,timeout] () {
            this->process_kernels();
            this->handle_events();
            this->flush_buffers(timeout);
            return this->stopping();
        };
        if (timeout) {
            process();
        } else {
            this->poller().wait(lock, process);
            /*
            this->poller().wait(
                lock,
                [this] () { return this->stopping(); }
            );*/
        }
    }
}

void sbn::basic_socket_pipeline::flush_buffers(bool timeout) {
    const time_point now = timeout
                           ? clock_type::now()
                           : time_point(duration::zero());
    handler_const_iterator first = this->_connections.begin();
    handler_const_iterator last = this->_connections.end();
    while (first != last) {
        connection& h = *first->second;
        if (h.stopped() || (timeout && this->is_timed_out(h, now))) {
            this->log("remove _ (_)", h.socket_address(),
                      h.stopped() ? "stop" : "timeout");
            h.remove(this->poller());
            first = this->_connections.erase(first);
        } else {
            try {
                first->second->flush();
                ++first;
            } catch (const std::exception& err) {
                this->log("remove _ (_)", h.socket_address(), err.what());
                h.remove(poller());
                first = this->_connections.erase(first);
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
    for (auto& pair : this->_connections) { pair.second->clear(); }
}
