#ifndef APPS_FACTORY_RESIDENT_KERNEL_HH
#define APPS_FACTORY_RESIDENT_KERNEL_HH

#include <factory/api.hh>

namespace factory {

	class resident_kernel: public factory::api::Kernel {

	private:
		struct start_message: public factory::api::Kernel {};
		struct stop_message: public factory::api::Kernel {};

	public:

		inline void
		start() {
			using namespace factory::api;
			send<Local>(new start_message, this);
		}

		inline void
		stop() {
			using namespace factory::api;
			send<Local>(new stop_message, this);
		}

		void
		act() override;

		void
		react(factory::api::Kernel* kernel) override;

	protected:

		virtual void
		on_start();

		virtual void
		on_stop();

		virtual void
		on_kernel(factory::api::Kernel* kernel);

	};

}

#endif // vim:filetype=cpp
