#ifndef FACTORY_KERNEL_ACT_HH
#define FACTORY_KERNEL_ACT_HH

#include <factory/kernel/kernel.hh>
#include <factory/ppl/basic_pipeline.hh>

namespace factory {

	inline void
	act(Kernel* kernel) {
		if (kernel->result() == exit_code::undefined) {
			if (kernel->principal()) {
				kernel->principal()->react(kernel);
			} else {
				kernel->act();
			}
		} else {
			bool del = false;
			if (!kernel->principal()) {
				del = !kernel->parent();
			} else {
				del = *kernel->principal() == *kernel->parent();
				if (kernel->result() == exit_code::success) {
					kernel->principal()->react(kernel);
				} else {
					kernel->principal()->error(kernel);
				}
			}
			if (del) {
				#ifdef SPRINGY
				::springy::graph.remove_node(kernel->unique_id());
				#endif
				delete kernel;
			}
		}
	}

}

#endif // vim:filetype=cpp
