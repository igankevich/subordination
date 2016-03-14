#ifndef TEST_BIG_KERNEL_HH
#define TEST_BIG_KERNEL_HH

#include "datum.hh"

template<uint32_t Size>
struct Big_kernel: public Kernel {

	Big_kernel():
	_data(Size)
	{}

	void act() override {
		commit(remote_server());
	}

	void
	write(sysx::packetstream& out) override {
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void
	read(sysx::packetstream& in) override {
		Kernel::read(in);
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum>
	data() const noexcept {
		return _data;
	}

	bool
	operator==(const Big_kernel& rhs) const noexcept {
		return _data.size() == rhs._data.size()
			and std::equal(_data.begin(), _data.end(),
				rhs._data.begin());
	}

	bool
	operator!=(const Big_kernel& rhs) const noexcept {
		return !operator==(rhs);
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Big_kernel& rhs) {
		for (const Datum& x : rhs._data) {
			out << sysx::make_bytes(x);
		}
		return out;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			12345u + Size,
			"Big_kernel",
			[] (sysx::packetstream& in) {
				Big_kernel<Size>* k = new Big_kernel<Size>;
				k->read(in);
				return k;
			}
		};
	}

private:

	std::vector<Datum> _data;

};

#endif // TEST_BIG_KERNEL_HH
