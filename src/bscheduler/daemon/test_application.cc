#include <bscheduler/api.hh>
#include <bscheduler/base/error_handler.hh>

#include "test_application.hh"

class slave_kernel: public bsc::kernel {

private:
	uint32_t _number = 0;
	uint32_t _nslaves = 0;

public:

	slave_kernel() = default;

	explicit
	slave_kernel(uint32_t number, uint32_t nslaves):
	_number(number),
	_nslaves(nslaves)
	{}

	void
	act() override {
		sys::log_message("slave", "act [_/_]", this->_number, this->_nslaves);
		bsc::commit<bsc::Remote>(this);
	}

	void
	write(sys::pstream& out) const override {
		bsc::kernel::write(out);
		out << this->_number << this->_nslaves;
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
		in >> this->_number >> this->_nslaves;
	}

	inline uint32_t
	number() const noexcept {
		return this->_number;
	}

};

class master_kernel: public bsc::kernel {

private:
	uint32_t _nkernels = BSCHEDULER_TEST_NUM_KERNELS;
	uint32_t _nreturned = 0;

public:

	void
	act() override {
		sys::log_message("master", "start");
		for (uint32_t i=0; i<this->_nkernels; ++i) {
			bsc::upstream<bsc::Remote>(
				this,
				new slave_kernel(i+1, this->_nkernels)
			);
		}
	}

	void
	react(bsc::kernel* child) {
		slave_kernel* k = dynamic_cast<slave_kernel*>(child);
		if (k->from()) {
			sys::log_message(
				"master",
				"react [_/_] from _",
				k->number(),
				this->_nkernels,
				k->from()
			);
		} else {
			sys::log_message(
				"master",
				"react [_/_]",
				k->number(),
				this->_nkernels
			);
		}
		if (++this->_nreturned == this->_nkernels) {
			sys::log_message("master", "finish");
			bsc::commit(this);
		}
	}

};

int main(int argc, char* argv[]) {
	using namespace bsc;
	install_error_handler();
	register_type<slave_kernel>();
	factory_guard g;
	if (this_application::is_master()) {
		send(new master_kernel);
	}
	return wait_and_return();
}
