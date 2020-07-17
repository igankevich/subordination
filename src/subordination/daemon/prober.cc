#include <subordination/daemon/factory.hh>
#include <subordination/daemon/prober.hh>

void sbnd::prober::act() {
    this->send_probe(this->_newprinc);
}

void sbnd::prober::react(sbn::kernel_ptr&& k) {
    auto p = sbn::pointer_dynamic_cast<probe>(std::move(k));
    if (p->source() == this->_newprinc) {
        this->return_code(p->return_code());
        if (this->return_code() == sbn::exit_code::success &&
            this->_oldprinc && this->_oldprinc != this->_newprinc) {
            this->send_probe(this->_oldprinc);
        }
    }
    if (--this->_nprobes == 0) {
        this->return_to_parent();
        factory.local().send(std::move(this_ptr()));
    }
}

void sbnd::prober::send_probe(const sys::socket_address& dest) {
    ++this->_nprobes;
    sbn::kernel_ptr p(new probe(this->_ifaddr, this->_oldprinc, this->_newprinc));
    p->destination(dest);
    p->principal_id(1); // TODO
    p->parent(this);
    p->phase(sbn::kernel::phases::point_to_point);
    factory.remote().send(std::move(p));
}
