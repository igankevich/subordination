#include "parallel_pipeline.hh"
#include "config.hh"

#include <bscheduler/kernel/act.hh>
#include <unistdx/util/backtrace>

template <class T>
void
bsc::parallel_pipeline<T>::do_run() {
	lock_type lock(this->_mutex);
	this->_semaphore.wait(lock, [this,&lock] () {
		while (!this->_kernels.empty()) {
			kernel_type* k = traits_type::front(this->_kernels);
			traits_type::pop(this->_kernels);
			sys::unlock_guard<lock_type> g(lock);
			try {
				::bsc::act(k);
			} catch (...) {
				sys::backtrace(2);
				throw;
			}
		}
		return this->has_stopped();
	});
}

template class bsc::parallel_pipeline<BSCHEDULER_KERNEL_TYPE>;
