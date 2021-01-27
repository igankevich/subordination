#include <unistdx/ipc/process>
#include <unistdx/system/error>

#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/parallel_pipeline.hh>

namespace {

    template <class T>
    struct Front {
        inline static typename T::const_reference
        front(const T& container) { return container.front(); }
        inline static typename T::reference
        front(T& container) { return container.front(); }
    };

    template <class A, class B, class C>
    struct Front<std::priority_queue<A,B,C>> {
        inline static typename std::priority_queue<A,B,C>::const_reference
        front(const std::priority_queue<A,B,C>& container) { return container.top(); }
        inline static typename std::priority_queue<A,B,C>::reference
        front(std::priority_queue<A,B,C>& container) { return const_cast<A&>(container.top()); }
    };

    template <class T, class Sack> inline void
    clear_queue(T& queue, Sack& sack) {
        while (!queue.empty()) {
            auto& k = Front<T>::front(queue);
            k.release()->mark_as_deleted(sack);
            queue.pop();
        }
    }

    inline void act(sbn::kernel_ptr& k) {
        #if defined(SBN_DEBUG)
        sys::log_message("act", "_", *k);
        #endif
        switch (k->phase()) {
            case sbn::kernel::phases::upstream:
                k->this_ptr(&k);
                k->act();
                (void)k.release();
                break;
            case sbn::kernel::phases::downstream:
                if (auto* p = k->principal()) {
                    sbn::kernel_ptr ptr(p);
                    p->this_ptr(&ptr);
                    p->react(std::move(k));
                    (void)ptr.release();
                } else {
                    #if defined(SBN_DEBUG)
                    sys::log_message("act", "shutdown after _", *k);
                    #endif
                    sbn::exit(int(k->return_code()));
                }
                break;
            case sbn::kernel::phases::point_to_point:
                if (auto* p = k->principal()) {
                    sbn::kernel_ptr ptr(p);
                    p->this_ptr(&ptr);
                    p->react(std::move(k));
                    (void)ptr.release();
                } else {
                    throw std::invalid_argument("point-to-point kernel without target");
                }
                break;
            case sbn::kernel::phases::broadcast:
                k->this_ptr(&k);
                k->act();
                break;
            default:
                throw std::invalid_argument("unknown phase");
        }
    }

    inline void process_kernel(sbn::kernel_ptr&& k, sbn::parallel_pipeline* this_pipeline) {
        try {
            act(k);
        } catch (const std::exception& err) {
            sys::backtrace(2);
            this_pipeline->log("error _", err.what());
            if (k) {
                k->rollback();
                auto* error_pipeline = this_pipeline->error_pipeline();
                if (error_pipeline) {
                    k->return_to_parent(sbn::exit_code::error);
                    error_pipeline->send(std::move(k));
                }
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
            auto k = std::move(queue.front());
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            process_kernel(std::move(k), this);
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
        while (!stopping() && (queue.empty() || queue.top()->at() > clock_type::now())) {
            const auto t = queue.empty() ? time_point(duration::max()) : queue.top()->at();
            this->_timer_semaphore.wait_until(lock, t);
        }
        if (!queue.empty() && queue.top()->at() <= clock_type::now()) {
            auto k = std::move(const_cast<kernel_ptr&>(queue.top()));
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            process_kernel(std::move(k), this);
        }
    }
}

void sbn::parallel_pipeline::downstream_loop(kernel_queue& queue, semaphore_type& semaphore) {
    lock_type lock(this->_mutex);
    semaphore.wait(lock, [this,&lock,&queue] () {
        while (!queue.empty()) {
            auto k = std::move(queue.front());
            queue.pop();
            sys::unlock_guard<lock_type> g(lock);
            process_kernel(std::move(k), this);
        }
        return this->stopping();
    });
}

void sbn::parallel_pipeline::kernel_loop(kernel_ptr k) {
    try {
        k->this_ptr(&k);
        k->act();
    } catch (const std::exception& err) {
        log("error _", err.what());
        if (k) {
            k->rollback();
            if (this->_error_pipeline) {
                k->return_to_parent(sbn::exit_code::error);
                this->_error_pipeline->send(std::move(k));
            }
        }
    }
}

void sbn::parallel_pipeline::start() {
    lock_type lock(this->_mutex);
    this->setstate(states::starting);
    const auto num_upstream_threads = this->_upstream_threads.size();
    const auto num_downstream_threads = this->_downstream_threads.size();
    if (num_downstream_threads == 0) {
        this->_downstream_kernels = kernel_queue_array(num_upstream_threads);
    }
    for (size_t i=0; i<num_upstream_threads; ++i) {
        this->_upstream_threads[i] = std::thread([this,i] () {
            #if defined(UNISTDX_HAVE_PRCTL)
            ::prctl(PR_SET_NAME, this->_name);
            #endif
            if (this->_thread_init) { this->_thread_init(i); }
            this->upstream_loop(this->_downstream_kernels[i]);
        });
    }
    this->_timer_thread = std::thread([this] () {
        #if defined(UNISTDX_HAVE_PRCTL)
        ::prctl(PR_SET_NAME, this->_name);
        #endif
        if (this->_thread_init) { this->_thread_init(0); }
        this->timer_loop();
    });
    for (size_t i=0; i<num_downstream_threads; ++i) {
        this->_downstream_threads[i] = std::thread([this,i] () {
            #if defined(UNISTDX_HAVE_PRCTL)
            ::prctl(PR_SET_NAME, this->_name);
            #endif
            if (this->_thread_init) { this->_thread_init(i); }
            this->downstream_loop(this->_downstream_kernels[i],
                                  this->_downstream_semaphores[i]);
        });
    }
    this->setstate(states::started);
}

void sbn::parallel_pipeline::stop() {
    lock_type lock(this->_mutex);
    this->setstate(states::stopping);
    this->_upstream_semaphore.notify_all();
    this->_timer_semaphore.notify_all();
    for (auto& s : this->_downstream_semaphores) { s.notify_all(); }
}

void sbn::parallel_pipeline::wait() {
    for (auto& t : this->_upstream_threads) { if (t.joinable()) { t.join(); } }
    if (this->_timer_thread.joinable()) { this->_timer_thread.join(); }
    for (auto& t : this->_downstream_threads) { if (t.joinable()) { t.join(); } }
    for (auto& t : this->_kernel_threads) { if (t.joinable()) { t.join(); } }
    lock_type lock(this->_mutex);
    this->setstate(states::stopped);
}

void sbn::parallel_pipeline::clear(kernel_sack& sack) {
    clear_queue(this->_upstream_kernels, sack);
    clear_queue(this->_timer_kernels, sack);
    for (auto& queue : this->_downstream_kernels) { clear_queue(queue, sack); }
}

void sbn::parallel_pipeline::num_downstream_threads(size_t n) {
    switch (state()) {
        case states::initial: break;
        case states::stopped: break;
        default: if (!this->_downstream_threads.empty()) {
                     throw std::runtime_error("invalid pipeline state");
                 }
    }
    this->_downstream_kernels = kernel_queue_array(n);
    this->_downstream_semaphores = semaphore_array(n);
}

void sbn::parallel_pipeline::num_upstream_threads(size_t n) {
    switch (state()) {
        case states::initial: break;
        case states::stopped: break;
        default: if (!this->_upstream_threads.empty()) {
                     throw std::runtime_error("invalid pipeline state");
                 }
    }
    this->_upstream_threads.resize(n);
}
