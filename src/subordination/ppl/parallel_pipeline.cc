#include <unistdx/ipc/process>
#include <unistdx/util/backtrace>

#include <subordination/kernel/act.hh>
#include <subordination/ppl/parallel_pipeline.hh>

void sbn::parallel_pipeline::upstream_loop(kernel_queue& downstream_queue) {
    lock_type lock(this->_mutex);
    this->_upstream_semaphore.wait(lock, [this,&lock,&downstream_queue] () {
        if (this->_downstream_threads.empty()) {
            while (!downstream_queue.empty()) {
                auto* k = downstream_queue.front();
                downstream_queue.pop();
                sys::unlock_guard<lock_type> g(lock);
                try {
                    ::sbn::act(k);
                } catch (...) {
                    sys::backtrace(2);
                    throw; // TODO send back to parent
                }
            }
        }
        auto& queue = this->_upstream_kernels;
        while (!queue.empty()) {
            auto* k = queue.front();
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            try {
                ::sbn::act(k);
            } catch (...) {
                sys::backtrace(2);
                throw; // TODO send back to parent
            }
        }
        return this->stopped();
    });
}

void sbn::parallel_pipeline::timer_loop() {
    using duration = kernel::duration;
    using time_point = kernel::time_point;
    lock_type lock(this->_mutex);
    auto& queue = this->_timer_kernels;
    while (!stopped()) {
        const auto timeout = queue.empty() ? time_point(duration::max()) : queue.top()->at();
        this->_timer_semaphore.wait_until(lock, timeout,
            [&] () { return stopped() || !queue.empty(); });
        if (!queue.empty()) {
            auto* k = queue.top();
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            try {
                ::sbn::act(k);
            } catch (...) {
                sys::backtrace(2);
                throw; // TODO send back to parent
            }
        }
    }
}

void sbn::parallel_pipeline::downstream_loop(kernel_queue& queue, semaphore_type& semaphore) {
    lock_type lock(this->_mutex);
    semaphore.wait(lock, [this,&lock,&queue] () {
        while (!queue.empty()) {
            auto* k = queue.front();
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            try {
                ::sbn::act(k);
            } catch (...) {
                sys::backtrace(2);
                throw; // TODO send back to parent
            }
        }
        return this->stopped();
    });
}

void sbn::parallel_pipeline::start() {
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::starting);
    const auto num_upstream_threads = this->_upstream_threads.size();
    for (size_t i=0; i<num_upstream_threads; ++i) {
        this->_upstream_threads[i] = std::thread([this,i] () {
            #if defined(UNISTDX_HAVE_PRCTL)
            ::prctl(PR_SET_NAME, this->_name);
            #endif
            this->upstream_loop(this->_downstream_kernels[i]);
        });
    }
    this->_timer_thread = std::thread([this] () {
        #if defined(UNISTDX_HAVE_PRCTL)
        ::prctl(PR_SET_NAME, this->_name);
        #endif
        this->timer_loop();
    });
    const auto num_downstream_threads = this->_upstream_threads.size();
    for (size_t i=0; i<num_downstream_threads; ++i) {
        this->_downstream_threads[i] = std::thread([this,i] () {
            #if defined(UNISTDX_HAVE_PRCTL)
            ::prctl(PR_SET_NAME, this->_name);
            #endif
            this->downstream_loop(this->_downstream_kernels[i],
                                  this->_downstream_semaphores[i]);
        });
    }
    this->setstate(pipeline_state::started);
}

void sbn::parallel_pipeline::stop() {
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopping);
    this->_upstream_semaphore.notify_all();
    this->_timer_semaphore.notify_all();
    for (auto& s : this->_downstream_semaphores) { s.notify_all(); }
}

void sbn::parallel_pipeline::wait() {
    for (auto& t : this->_upstream_threads) { if (t.joinable()) { t.join(); } }
    auto& t = this->_timer_thread;
    if (t.joinable()) { t.join(); }
    for (auto& t : this->_downstream_threads) { if (t.joinable()) { t.join(); } }
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopped);
}

void sbn::parallel_pipeline::clear() {
    std::vector<std::unique_ptr<kernel>> sack;
    {
        auto& queue = this->_upstream_kernels;
        while (!queue.empty()) {
            queue.front()->mark_as_deleted(sack);
            queue.pop();
        }
    }
    {
        auto& queue = this->_timer_kernels;
        while (!queue.empty()) {
            queue.top()->mark_as_deleted(sack);
            queue.pop();
        }
    }
    {
        for (auto& queue : this->_downstream_kernels) {
            while (!queue.empty()) {
                queue.front()->mark_as_deleted(sack);
                queue.pop();
            }
        }
    }
}

void sbn::parallel_pipeline::num_downstream_threads(size_t n) {
    if (!this->_downstream_threads.empty()) {
        throw std::invalid_argument("downstream threads must be empty");
    }
    this->_downstream_threads.resize(n);
    this->_downstream_kernels.resize(n);
    this->_downstream_semaphores = semaphore_array(n);
}

void sbn::parallel_pipeline::num_upstream_threads(size_t n) {
    if (!this->_upstream_threads.empty()) {
        throw std::invalid_argument("upstream threads must be empty");
    }
    this->_upstream_threads.resize(n);
}
