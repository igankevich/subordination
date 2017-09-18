#include "prober.hh"

void
factory::prober::act() {
	this->send_probe(this->_newprinc);
}

void
factory::prober::react(factory::api::Kernel* k) {
	probe* p = dynamic_cast<probe*>(k);
	if (p->from() == this->_newprinc) {
		this->result(p->result());
		if (this->result() == exit_code::success && this->_oldprinc) {
			this->send_probe(this->_oldprinc);
		}
	}
	if (--this->_nprobes == 0) {
		factory::api::commit<factory::api::Local>(this);
	}
}

void
factory::prober::send_probe(const sys::endpoint& dest) {
	++this->_nprobes;
	probe* p = new probe(this->_ifaddr, this->_oldprinc, this->_newprinc);
	p->to(dest);
	p->set_principal_id(1);
	factory::api::upstream<factory::api::Remote>(this, p);
}
