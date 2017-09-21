#ifndef APPS_FACTORY_RESIDENT_KERNEL_HH
#define APPS_FACTORY_RESIDENT_KERNEL_HH

#include <factory/api.hh>

namespace asc {

	class resident_kernel: public asc::kernel {

	private:
		struct start_message: public asc::kernel {};
		struct stop_message: public asc::kernel {};

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
		react(asc::kernel* k) override;

	protected:

		virtual void
		on_start();

		virtual void
		on_stop();

		virtual void
		on_kernel(asc::kernel* k);

	};

}

#endif // vim:filetype=cpp
