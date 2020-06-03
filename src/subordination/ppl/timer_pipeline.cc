#include <chrono>
#include <subordination/ppl/timer_pipeline.hh>
#include <unistdx/base/unlock_guard>
#include <unistdx/util/backtrace>

void sbn::timer_pipeline::do_run() {
    using duration = kernel::duration;
    using time_point = kernel::time_point;
    lock_type lock(this->_mutex);
    while (!this->stopped()) {
        const auto timeout = this->_kernels.empty()
            ? time_point(duration::max())
            : this->_kernels.top()->at();
        this->_semaphore.wait_until(lock, timeout,
            [this] () { return this->stopped() || !this->_kernels.empty(); });
        if (!this->_kernels.empty()) {
            auto* k = this->_kernels.top();
            this->_kernels.pop();
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

void sbn::timer_pipeline::collect_kernels(kernel_sack& sack) {
    while (!this->_kernels.empty()) {
        this->_kernels.top()->mark_as_deleted(sack);
        this->_kernels.pop();
    }
}
