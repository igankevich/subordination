#include "resident_kernel.hh"

#include <typeinfo>

void
factory::resident_kernel::act() {
	this->start();
}

void
factory::resident_kernel::react(factory::api::Kernel* kernel) {
	using namespace factory::api;
	if (typeid(*kernel) == typeid(start_message)) {
		delete kernel;
		this->on_start();
	} else if (typeid(*kernel) == typeid(stop_message)) {
		delete kernel;
		this->on_stop();
		if (this->parent()) {
			commit<Local>(this, exit_code::success);
		}
	} else {
		this->on_kernel(kernel);
	}
}

void
factory::resident_kernel::on_start() {}

void
factory::resident_kernel::on_stop() {}

void
factory::resident_kernel::on_kernel(factory::api::Kernel* kernel) {}
