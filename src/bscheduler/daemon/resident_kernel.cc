#include "resident_kernel.hh"

#include <typeinfo>

void
bsc::resident_kernel::act() {
	this->start();
}

void
bsc::resident_kernel::react(bsc::kernel* k) {
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
bsc::resident_kernel::on_start() {}

void
bsc::resident_kernel::on_stop() {}

void
bsc::resident_kernel::on_kernel(bsc::kernel* k) {}
