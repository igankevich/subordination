#include <subordination/ppl/parallel_pipeline.hh>

#include <subordination/kernel/act.hh>
#include <unistdx/util/backtrace>

void sbn::parallel_pipeline::do_run() {
    lock_type lock(this->_mutex);
    this->_semaphore.wait(lock, [this,&lock] () {
        while (!this->_kernels.empty()) {
            auto* k = this->_kernels.front();
            this->_kernels.pop();
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

void sbn::parallel_pipeline::stop() {
    lock_type lock(this->_mutex);
    sbn::basic_pipeline::stop();
    this->_semaphore.notify_all();
}

void sbn::parallel_pipeline::collect_kernels(kernel_sack& sack) {
    while (!this->_kernels.empty()) {
        this->_kernels.front()->mark_as_deleted(sack);
        this->_kernels.pop();
    }
}
