#ifndef FACTORY_ALGORITHM_HH
#define FACTORY_ALGORITHM_HH

#include <factory/kernel/kernel.hh>
#include <factory/ppl/basic_server.hh>

namespace factory {

	inline void
	act(Kernel* kernel) {
		if (kernel->result() == Result::undefined) {
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
				if (kernel->result() == Result::success) {
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

#endif // FACTORY_ALGORITHM_HH
