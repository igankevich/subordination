#include <subordination/ppl/parallel_pipeline.hh>
#include <subordination/config.hh>

#include <subordination/kernel/act.hh>
#include <unistdx/util/backtrace>

template <class T>
void
sbn::parallel_pipeline<T>::do_run() {
    lock_type lock(this->_mutex);
    this->_semaphore.wait(lock, [this,&lock] () {
        while (!this->_kernels.empty()) {
            kernel_type* k = traits_type::front(this->_kernels);
            traits_type::pop(this->_kernels);
            sys::unlock_guard<lock_type> g(lock);
            try {
                ::sbn::act(k);
            } catch (...) {
                sys::backtrace(2);
                throw;
            }
        }
        return this->has_stopped();
    });
}

template class sbn::parallel_pipeline<SUBORDINATION_KERNEL_TYPE>;
