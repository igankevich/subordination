#include <typeinfo>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/resident_kernel.hh>

void sbnd::resident_kernel::start() {
    auto* m = new start_message;
    m->principal(this);
    factory.local().send(m);
}

void sbnd::resident_kernel::stop() {
    auto* m = new stop_message;
    m->principal(this);
    factory.local().send(m);
}

void
sbnd::resident_kernel::act() {
    this->start();
}

void
sbnd::resident_kernel::react(sbn::kernel* k) {
    if (typeid(*k) == typeid(start_message)) {
        this->on_start();
    } else if (typeid(*k) == typeid(stop_message)) {
        this->on_stop();
        if (this->parent()) {
            this->return_to_parent(sbn::exit_code::success);
            factory.local().send(this);
        }
    } else {
        this->on_kernel(k);
    }
}

void sbnd::resident_kernel::on_start() {}
void sbnd::resident_kernel::on_stop() {}
void sbnd::resident_kernel::on_kernel(sbn::kernel* k) {}
