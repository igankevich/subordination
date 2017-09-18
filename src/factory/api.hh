#ifndef FACTORY_API_HH
#define FACTORY_API_HH

#include <factory/ppl/basic_factory.hh>
#include <factory/config.hh>

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

		template<Target target=Target::Local>
		void
		upstream(Kernel* lhs, Kernel* rhs) {
			rhs->parent(lhs);
			send<target>(rhs);
		}

		template<Target target>
		void
		commit(Kernel* rhs, exit_code ret) {
			if (!rhs->parent()) {
				delete rhs;
				factory::graceful_shutdown(static_cast<int>(ret));
			} else {
				rhs->return_to_parent(ret);
				send<target>(rhs);
			}
		}

		template<Target target>
		void
		commit(Kernel* rhs) {
			exit_code ret = rhs->result();
			commit<target>(rhs, ret == exit_code::undefined ? exit_code::success : ret);
		}

		template<Target target=Target::Local>
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
		ppl.send(rhs);
	}

}

#endif // vim:filetype=cpp
