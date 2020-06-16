#include <subordination/daemon/factory.hh>
#include <subordination/daemon/prober.hh>
#include <unistdx/base/log_message>

void
sbnd::prober::act() {
    this->send_probe(this->_newprinc);
}

void
sbnd::prober::react(sbn::kernel* k) {
    probe* p = dynamic_cast<probe*>(k);
    if (p->source() == this->_newprinc) {
        this->return_code(p->return_code());
        if (this->return_code() == sbn::exit_code::success && this->_oldprinc) {
            this->send_probe(this->_oldprinc);
        }
    }
    if (--this->_nprobes == 0) {
        this->return_to_parent(sbn::exit_code::success);
        factory.local().send(this);
    }
}

void
sbnd::prober::send_probe(const sys::socket_address& dest) {
    ++this->_nprobes;
    probe* p = new probe(this->_ifaddr, this->_oldprinc, this->_newprinc);
    p->destination(dest);
    p->principal_id(1); // TODO
    p->parent(this);
    factory.remote().send(p);
}
