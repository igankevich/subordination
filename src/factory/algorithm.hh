#ifndef FACTORY_ALGORITHM_HH
#define FACTORY_ALGORITHM_HH

#include <factory/kernel.hh>
#include <factory/result.hh>
#ifdef SPRINGY
#include <springy/springy.hh>
#endif

namespace factory {

	#ifdef SPRINGY
	springy::Springy_graph<Kernel> graph;
	#endif

	template<class Pipeline>
	void
	upstream(Pipeline& ppl, Kernel* lhs, Kernel* rhs) {
		rhs->parent(lhs);
		#ifdef SPRINGY
		if (lhs) {
			rhs->_mass = 0.9 * lhs->_mass;
		}
		graph.add_node(*rhs);
		if (lhs) {
			graph.add_edge(*lhs, *rhs);
		}
		#endif
		ppl.send(rhs);
	}

	template<class Pipeline>
	void
	upstream_carry(Pipeline& ppl, Kernel* lhs, Kernel* rhs) {
		rhs->setf(Kernel::Flag::carries_parent);
		upstream(ppl, lhs, rhs);
	}

	template<class Pipeline>
	void
	commit(Pipeline& ppl, Kernel* rhs, Result ret) {
		if (!rhs->parent()) {
			#ifdef SPRINGY
			graph.remove_node(*rhs);
			#endif
			delete rhs;
			factory::graceful_shutdown(static_cast<int>(ret));
		} else {
			rhs->return_to_parent(ret);
			#ifdef SPRINGY
			graph.add_edge(*rhs, *rhs->parent());
			#endif
			ppl.send(rhs);
		}
	}

	template<class Pipeline>
	void
	commit(Pipeline& ppl, Kernel* rhs) {
		Result ret = rhs->result();
		commit(ppl, rhs, ret == Result::undefined ? Result::success : ret);
	}

	template<class Pipeline>
	void
	send(Pipeline& ppl, Kernel* lhs, Kernel* rhs) {
		lhs->principal(rhs);
		ppl.send(lhs);
	}

	void
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
				graph.remove_node(*kernel);
				#endif
				delete kernel;
			}
		}
	}

}

#endif // FACTORY_ALGORITHM_HH
