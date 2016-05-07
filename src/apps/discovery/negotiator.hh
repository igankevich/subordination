#ifndef APPS_DISCOVERY_NEGOTIATOR_HH
#define APPS_DISCOVERY_NEGOTIATOR_HH

#include "hierarchy.hh"

template<class Address>
struct Negotiator: public Priority_kernel<Kernel> {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;
	typedef stdx::log<Negotiator> this_log;

	Negotiator() noexcept:
	_oldprinc(),
	_newprinc()
	{}

	Negotiator(sys::endpoint old, sys::endpoint neww) noexcept:
	_oldprinc(old),
	_newprinc(neww)
	{}

	template<class Hierarchy>
	void
	negotiate(Hierarchy& hierarchy) {
		const sys::endpoint& this_addr = hierarchy.bindaddr();
		this->principal(this->parent());
		this->result(Result::success);
		if (_newprinc == this_addr) {
			// principal becomes subordinate
			if (this->from() == hierarchy.principal()) {
				if (_oldprinc) {
					hierarchy.unset_principal();
				} else {
					// root tries to swap with its subordinate
					this->result(Result::error);
				}
			}
			if (this->result() != Result::error) {
				hierarchy.add_subordinate(this->from());
			}
		} else
		if (_oldprinc == this_addr) {
			// something fancy is going on
			if (this->from() == hierarchy.principal()) {
				this->result(Result::error);
			} else {
				hierarchy.remove_subordinate(this->from());
			}
		}
		remote_server.send(this);
	}

	void write(sys::packetstream& out) override {
		Kernel::write(out);
		// TODO: if moves_upstream
		out << _oldprinc << _newprinc;
	}

	void read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> _oldprinc >> _newprinc;
	}

private:

	sys::endpoint _oldprinc;
	sys::endpoint _newprinc;

};

template<class Address>
struct Master_negotiator: public Priority_kernel<Kernel> {

	typedef Address addr_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef stdx::log<Master_negotiator> this_log;

	Master_negotiator(sys::endpoint oldp, sys::endpoint newp):
	_oldprinc(oldp),
	_newprinc(newp)
	{}

	void
	act() override {
		send_negotiator(_newprinc);
	}

	void
	react(Kernel* k) override {
		bool finished = true;
		if (_numsent == 1) {
			this_log() << "Tried " << k->from() << ": " << k->result() << std::endl;
			this->result(k->result());
			if (k->result() == Result::success and _oldprinc) {
				finished = false;
				send_negotiator(_oldprinc);
			}
		}
		if (finished) {
			this->principal(this->parent());
			local_server.send(this);
		}
	}

	const sys::endpoint&
	old_principal() const noexcept {
		return _oldprinc;
	}

	const sys::endpoint&
	new_principal() const noexcept {
		return _newprinc;
	}

private:

	void
	send_negotiator(sys::endpoint addr) {
		++_numsent;
		negotiator_type* n = new negotiator_type(_oldprinc, _newprinc);
		n->set_principal_id(addr.address());
		n->to(addr);
		upstream(remote_server, this, n);
	}

	sys::endpoint _oldprinc;
	sys::endpoint _newprinc;
	uint32_t _numsent = 0;
};

#endif // APPS_DISCOVERY_NEGOTIATOR_HH
