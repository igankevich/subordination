#ifndef APPS_DISCOVERY_NEGOTIATOR_HH
#define APPS_DISCOVERY_NEGOTIATOR_HH

#include "hierarchy.hh"
#include <factory/api.hh>

template<class Address>
struct Negotiator: public Priority_kernel<Kernel> {

	typedef Address addr_type;
	typedef discovery::Hierarchy<addr_type> hierarchy_type;

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
		using namespace factory::api;
		const sys::endpoint& this_addr = hierarchy.bindaddr();
		this->principal(this->parent());
		this->result(exit_code::success);
		if (_newprinc == this_addr) {
			// principal becomes subordinate
			if (this->from() == hierarchy.principal()) {
				if (_oldprinc) {
					hierarchy.unset_principal();
				} else {
					// root tries to swap with its subordinate
					this->result(exit_code::error);
				}
			}
			if (this->result() != exit_code::error) {
				hierarchy.add_subordinate(this->from());
			}
		} else
		if (_oldprinc == this_addr) {
			// something fancy is going on
			if (this->from() == hierarchy.principal()) {
				this->result(exit_code::error);
			} else {
				hierarchy.remove_subordinate(this->from());
			}
		}
		send<Remote>(this);
	}

	void write(sys::pstream& out) override {
		Kernel::write(out);
		out << _oldprinc << _newprinc;
	}

	void read(sys::pstream& in) override {
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
	typedef sys::ipaddr_traits<addr_type> traits_type;
	typedef Negotiator<addr_type> negotiator_type;

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
			#ifndef NDEBUG
			sys::log_message("dscvr", "tried _, result ", k->from(), k->result());
			#endif
			this->result(k->result());
			if (k->result() == exit_code::success and _oldprinc) {
				finished = false;
				send_negotiator(_oldprinc);
			}
		}
		if (finished) {
			this->principal(this->parent());
			factory::api::send<Local>(this);
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
	send_negotiator(const sys::endpoint& addr) {
		using namespace factory::api;
		++_numsent;
		negotiator_type* n = new negotiator_type(_oldprinc, _newprinc);
		n->set_principal_id(traits_type::address(addr).rep());
		n->to(addr);
		upstream<Remote>(this, n);
	}

	sys::endpoint _oldprinc;
	sys::endpoint _newprinc;
	uint32_t _numsent = 0;
};

#endif // vim:filetype=cpp
