#include "resident_kernel.hh"

#include <typeinfo>

void
asc::resident_kernel::act() {
	this->start();
}

void
asc::resident_kernel::react(asc::kernel* k) {
	if (typeid(*k) == typeid(start_message)) {
		this->on_start();
	} else if (typeid(*k) == typeid(stop_message)) {
		this->on_stop();
		if (this->parent()) {
			commit<Local>(this, exit_code::success);
		}
	} else {
		this->on_kernel(k);
	}
}

void
asc::resident_kernel::on_start() {}

void
asc::resident_kernel::on_stop() {}

void
asc::resident_kernel::on_kernel(asc::kernel* k) {}
