#ifndef FACTORY_API_HH
#define FACTORY_API_HH

#include <factory/ppl/basic_factory.hh>
#include <factory/config.hh>
#ifdef SPRINGY
#include <springy/springy.hh>
#endif

namespace factory {

	/// Factory framework API.
	namespace api {

		typedef FACTORY_KERNEL_TYPE Kernel;

		enum Target {
			Local,
			Remote,
			Child
		};

		template <Target t>
		inline void
		send(Kernel* kernel) {
			factory.send(kernel);
		}

		template <>
		inline void
		send<Local>(Kernel* kernel) {
			factory.send(kernel);
		}

		template <>
		inline void
		send<Remote>(Kernel* kernel) {
			factory.send_remote(kernel);
		}

		template<Target target>
		void
		upstream(Kernel* lhs, Kernel* rhs) {
			rhs->parent(lhs);
			#ifdef SPRINGY
			if (lhs) {
				rhs->_mass = 0.9 * lhs->_mass;
			}
			::springy::graph.add_node(rhs->unique_id());
			if (lhs) {
				::springy::graph.add_edge(lhs->unique_id(), rhs->unique_id());
			}
			#endif
			send<target>(rhs);
		}

		template<Target target>
		void
		commit(Kernel* rhs, Result ret) {
			if (!rhs->parent()) {
				#ifdef SPRINGY
				::springy::graph.remove_node(rhs->unique_id());
				#endif
				delete rhs;
				factory::graceful_shutdown(static_cast<int>(ret));
			} else {
				rhs->return_to_parent(ret);
				#ifdef SPRINGY
				::springy::graph.add_edge(
					rhs->unique_id(),
					rhs->parent()->unique_id()
				);
				#endif
				send<target>(rhs);
			}
		}

		template<Target target>
		void
		commit(Kernel* rhs) {
			Result ret = rhs->result();
			commit<target>(rhs, ret == Result::undefined ? Result::success : ret);
		}

		template<Target target>
		void
		send(Kernel* lhs, Kernel* rhs) {
			lhs->principal(rhs);
			send<target>(lhs);
		}

		struct Factory_guard {

			inline
			Factory_guard() {
				::factory::factory.start();
			}

			inline
			~Factory_guard() {
				::factory::factory.stop();
				::factory::factory.wait();
			}

		};

	}

	template<class Pipeline>
	void
	upstream(Pipeline& ppl, Kernel* lhs, Kernel* rhs) {
		rhs->parent(lhs);
		#ifdef SPRINGY
		if (lhs) {
			rhs->_mass = 0.9 * lhs->_mass;
		}
		::springy::graph.add_node(*rhs);
		if (lhs) {
			::springy::graph.add_edge(*lhs, *rhs);
		}
		#endif
		ppl.send(rhs);
	}

}

#endif // FACTORY_API_HH vim:filetype=cpp
