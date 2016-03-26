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

	Negotiator(sysx::endpoint old, sysx::endpoint neww) noexcept:
	_oldprinc(old),
	_newprinc(neww)
	{}

	template<class Hierarchy>
	void
	negotiate(Server& this_server, Hierarchy& hierarchy) {
		const sysx::endpoint& this_addr = hierarchy.bindaddr();
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
		this_server.remote_server()->send(this);
	}

	void write(sysx::packetstream& out) override {
		Kernel::write(out);
		// TODO: if moves_upstream
		out << _oldprinc << _newprinc;
	}

	void read(sysx::packetstream& in) override {
		Kernel::read(in);
		in >> _oldprinc >> _newprinc;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			8,
			"Negotiator",
			[] (sysx::packetstream& in) {
				Negotiator<Address>* k = new Negotiator<Address>;
				k->read(in);
				return k;
			}
		};
	}

private:

	sysx::endpoint _oldprinc;
	sysx::endpoint _newprinc;

};

template<class Address>
struct Master_negotiator: public Priority_kernel<Kernel> {

	typedef Address addr_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef stdx::log<Master_negotiator> this_log;

	Master_negotiator(sysx::endpoint oldp, sysx::endpoint newp):
	_oldprinc(oldp),
	_newprinc(newp)
	{}

	void
	act(Server& this_server) override {
		send_negotiator(this_server, _newprinc);
	}

	void
	react(Server& this_server, Kernel* k) override {
		bool finished = true;
		if (_numsent == 1) {
			this_log() << "Tried " << k->from() << ": " << k->result() << std::endl;
			this->result(k->result());
			if (k->result() == Result::success and _oldprinc) {
				finished = false;
				send_negotiator(this_server, _oldprinc);
			}
		}
		if (finished) {
			this->principal(this->parent());
			local_server()->send(this);
		}
	}

	const sysx::endpoint&
	old_principal() const noexcept {
		return _oldprinc;
	}

	const sysx::endpoint&
	new_principal() const noexcept {
		return _newprinc;
	}

private:

	void
	send_negotiator(Server& this_server, sysx::endpoint addr) {
		++_numsent;
		negotiator_type* n = new negotiator_type(_oldprinc, _newprinc);
		n->set_principal_id(addr.address());
		n->to(addr);
		upstream(this_server.remote_server(), n);
	}

	sysx::endpoint _oldprinc;
	sysx::endpoint _newprinc;
	uint32_t _numsent = 0;
};

#endif // APPS_DISCOVERY_NEGOTIATOR_HH
