#include "prober.hh"

void
sbn::prober::act() {
    this->send_probe(this->_newprinc);
}

void
sbn::prober::react(sbn::kernel* k) {
    probe* p = dynamic_cast<probe*>(k);
    if (p->from() == this->_newprinc) {
        this->return_code(p->return_code());
        if (this->return_code() == exit_code::success && this->_oldprinc) {
            this->send_probe(this->_oldprinc);
        }
    }
    if (--this->_nprobes == 0) {
        sbn::commit<sbn::Local>(this);
    }
}

void
sbn::prober::send_probe(const sys::socket_address& dest) {
    ++this->_nprobes;
    probe* p = new probe(this->_ifaddr, this->_oldprinc, this->_newprinc);
    p->to(dest);
    p->set_principal_id(1);
    sbn::upstream<sbn::Remote>(this, p);
}
