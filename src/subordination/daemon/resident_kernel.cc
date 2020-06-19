#include <typeinfo>

#include <subordination/daemon/factory.hh>
#include <subordination/daemon/resident_kernel.hh>

void sbnd::resident_kernel::start() {
    auto m = sbn::make_pointer<start_message>();
    m->principal(this);
    m->phase(phases::point_to_point);
    factory.local().send(std::move(m));
}

void sbnd::resident_kernel::stop() {
    auto m = sbn::make_pointer<stop_message>();
    m->principal(this);
    m->phase(phases::point_to_point);
    factory.local().send(std::move(m));
}

void
sbnd::resident_kernel::act() {
    this->start();
}

void
sbnd::resident_kernel::react(sbn::kernel_ptr&& k) {
    if (typeid(*k) == typeid(start_message)) {
        this->on_start();
    } else if (typeid(*k) == typeid(stop_message)) {
        this->on_stop();
        if (this->parent()) {
            this->return_to_parent(sbn::exit_code::success);
            factory.local().send(std::move(this_ptr()));
        }
    } else {
        this->on_kernel(std::move(k));
    }
}

void sbnd::resident_kernel::on_start() {}
void sbnd::resident_kernel::on_stop() {}
void sbnd::resident_kernel::on_kernel(sbn::kernel_ptr&& k) {}
