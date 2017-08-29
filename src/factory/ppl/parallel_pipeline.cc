#include "parallel_pipeline.hh"
#include "config.hh"

#include <factory/kernel/algorithm.hh>

template <class T>
void
factory::parallel_pipeline<T>::do_run() {
	lock_type lock(this->_mutex);
	this->_semaphore.wait(lock, [this,&lock] () {
		while (!this->_kernels.empty()) {
			kernel_type* kernel = traits_type::front(this->_kernels);
			traits_type::pop(this->_kernels);
			sys::unlock_guard<lock_type> g(lock);
			::factory::act(kernel);
		}
		return this->is_stopped();
	});
}

template class factory::parallel_pipeline<FACTORY_KERNEL_TYPE>;