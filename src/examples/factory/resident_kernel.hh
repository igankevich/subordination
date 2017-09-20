#ifndef APPS_FACTORY_RESIDENT_KERNEL_HH
#define APPS_FACTORY_RESIDENT_KERNEL_HH

#include <factory/api.hh>

namespace asc {

	class resident_kernel: public asc::Kernel {

	private:
		struct start_message: public asc::Kernel {};
		struct stop_message: public asc::Kernel {};

	public:

		inline void
		start() {
			send<Local>(new start_message, this);
		}

		inline void
		stop() {
			send<Local>(new stop_message, this);
		}

		void
		act() override;

		void
		react(asc::Kernel* k) override;

	protected:

		virtual void
		on_start();

		virtual void
		on_stop();

		virtual void
		on_kernel(asc::Kernel* kernel);

	};

}

#endif // vim:filetype=cpp
