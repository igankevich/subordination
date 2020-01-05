#include "resident_kernel.hh"

#include <typeinfo>

void
sbn::resident_kernel::act() {
    this->start();
}

void
sbn::resident_kernel::react(sbn::kernel* k) {
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
sbn::resident_kernel::on_start() {}

void
sbn::resident_kernel::on_stop() {}

void
sbn::resident_kernel::on_kernel(sbn::kernel* k) {}
