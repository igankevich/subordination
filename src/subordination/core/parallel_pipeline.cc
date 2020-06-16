#include <unistdx/ipc/process>
#include <unistdx/util/backtrace>

#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/parallel_pipeline.hh>

namespace {

    template <class T>
    struct Front {
        inline static typename T::const_reference
        front(const T& container) { return container.front(); }
    };

    template <class A, class B, class C>
    struct Front<std::priority_queue<A,B,C>> {
        inline static typename std::priority_queue<A,B,C>::const_reference
        front(const std::priority_queue<A,B,C>& container) { return container.top(); }
    };

    template <class T, class Sack> inline void
    clear_queue(T& queue, Sack& sack) {
        while (!queue.empty()) {
            auto* k = Front<T>::front(queue);
            k->mark_as_deleted(sack);
            queue.pop();
        }
    }

    inline void act(sbn::kernel* k) {
        bool del = false;
        if (k->return_code() == sbn::exit_code::undefined) {
            if (k->principal()) {
                k->principal()->react(k);
                if (!k->isset(sbn::kernel_flag::do_not_delete)) {
                    del = true;
                } else {
                    k->unsetf(sbn::kernel_flag::do_not_delete);
                }
            } else {
                k->act();
            }
        } else {
            if (!k->principal()) {
                del = !k->parent();
                #if defined(SBN_DEBUG)
                sys::log_message("act", "shutdown after _", *k);
                #endif
                sbn::graceful_shutdown(int(k->return_code()));
            } else {
                del = *k->principal() == *k->parent();
                if (k->return_code() == sbn::exit_code::success) {
                    k->principal()->react(k);
                } else {
                    k->principal()->error(k);
                }
            }
        }
        if (del) { delete k; }
    }

    inline void process_kernel(sbn::kernel* k, sbn::pipeline* error_pipeline) {
        try {
            act(k);
        } catch (...) {
            k->rollback();
            sys::backtrace(2);
            if (error_pipeline) {
                k->return_to_parent(sbn::exit_code::error);
                error_pipeline->send(k);
            } else {
                delete k;
            }
        }
    }

}

void sbn::parallel_pipeline::upstream_loop(kernel_queue& downstream) {
    lock_type lock(this->_mutex);
    this->_upstream_semaphore.wait(lock, [this,&lock,&downstream] () {
        auto& upstream = this->_upstream_kernels;
        bool downstream_not_empty, upstream_not_empty;
        while ((downstream_not_empty = this->_downstream_threads.empty() &&
                                       !downstream.empty()) ||
               (upstream_not_empty = !upstream.empty())) {
            auto& queue = (downstream_not_empty ? downstream : upstream);
            auto* k = queue.front();
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            process_kernel(k, this->_error_pipeline);
        }
        return this->stopping();
    });
}

void sbn::parallel_pipeline::timer_loop() {
    using clock_type = kernel::clock_type;
    using duration = kernel::duration;
    using time_point = kernel::time_point;
    using namespace std::chrono;
    lock_type lock(this->_mutex);
    auto& queue = this->_timer_kernels;
    while (!stopping()) {
        while (!(stopping() || (!queue.empty() && queue.top()->at() <= clock_type::now()))) {
            const auto t = queue.empty() ? time_point(duration::max()) : queue.top()->at();
            this->_timer_semaphore.wait_until(lock, t);
        }
        if (!queue.empty() && queue.top()->at() <= clock_type::now()) {
            auto* k = queue.top();
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            process_kernel(k, this->_error_pipeline);
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
            process_kernel(k, this->_error_pipeline);
        }
        return this->stopping();
    });
}

void sbn::parallel_pipeline::start() {
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::starting);
    const auto num_upstream_threads = this->_upstream_threads.size();
    const auto num_downstream_threads = this->_downstream_threads.size();
    if (num_downstream_threads == 0) {
        this->_downstream_kernels.resize(num_upstream_threads);
    }
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
    if (this->_timer_thread.joinable()) { this->_timer_thread.join(); }
    for (auto& t : this->_downstream_threads) { if (t.joinable()) { t.join(); } }
    lock_type lock(this->_mutex);
    this->setstate(pipeline_state::stopped);
}

void sbn::parallel_pipeline::clear() {
    std::vector<std::unique_ptr<kernel>> sack;
    clear_queue(this->_upstream_kernels, sack);
    clear_queue(this->_timer_kernels, sack);
    for (auto& queue : this->_downstream_kernels) {
        clear_queue(queue, sack);
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
