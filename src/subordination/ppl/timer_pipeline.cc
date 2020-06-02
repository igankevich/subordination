#include <subordination/ppl/timer_pipeline.hh>

template <class T>
void
sbn::timer_pipeline<T>::do_run() {
    while (!this->has_stopped()) {
        lock_type lock(this->_mutex);
        this->wait_until_kernel_arrives(lock);
        if (!this->has_stopped()) {
            kernel_type* k = traits_type::front(this->_kernels);
            if (!this->wait_until_kernel_is_ready(lock, k)) {
                traits_type::pop(this->_kernels);
                lock.unlock();
                ::sbn::act(k);
            }
        }
    }
}

template class sbn::timer_pipeline<sbn::kernel>;
