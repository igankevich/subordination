#include <stdx/debug.hh>

#include <sys/fildesbuf.hh>

#include <factory/kernel.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>
#include <factory/reflection.hh>

#include "test.hh"
#include "datum.hh"

factory::Dummy_server remote_server;

#include "big_kernel.hh"

struct Good_kernel: public factory::Kernel {

	void
	write(sys::packetstream& out) override {
		factory::Kernel::write(out);
		out << _data;
	}

	void
	read(sys::packetstream& in) override {
		factory::Kernel::read(in);
		in >> _data;
	}

	bool
	operator==(const Good_kernel& rhs) const noexcept {
		return _data == rhs._data;
	}

	bool
	operator!=(const Good_kernel& rhs) const noexcept {
		return !operator==(rhs);
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Good_kernel& rhs) {
		return out << sys::make_bytes(rhs._data);
	}

	const Datum&
	data() const noexcept {
		return _data;
	}

private:

	Datum _data;

};

struct Kernel_that_writes_more_than_reads: public Good_kernel {

	void
	write(sys::packetstream& out) override {
		Good_kernel::write(out);
		// push dummy object to the stream
		out << Datum();
	}

};

struct Kernel_that_reads_more_than_writes: public Good_kernel {

	void
	read(sys::packetstream& in) override {
		Good_kernel::read(in);
		Datum dummy;
		// read dummy object from the stream
		in >> dummy;
	}

};

struct Kernel_that_carries_its_parent: public factory::Kernel {

	Kernel_that_carries_its_parent() {
		this->setf(factory::Kernel::Flag::carries_parent);
		this->parent(&_carry);
		assert(parent());
	}

	Kernel_that_carries_its_parent(int) {}

	void
	write(sys::packetstream& out) override {
		factory::Kernel::write(out);
		out << _data;
	}

	void
	read(sys::packetstream& in) override {
		factory::Kernel::read(in);
		in >> _data;
	}

	bool
	operator==(const Kernel_that_carries_its_parent& rhs) const noexcept {
		return _data == rhs._data and parent() and rhs.parent()
			and *good_parent() == *rhs.good_parent();
	}

	bool
	operator!=(const Kernel_that_carries_its_parent& rhs) const noexcept {
		return !operator==(rhs);
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Kernel_that_carries_its_parent& rhs) {
		out << sys::make_bytes(rhs._data) << ' ';
		if (rhs.good_parent()) {
			out << sys::make_bytes(rhs.good_parent()->data());
		} else {
			out << "nullptr";
		}
		return out;
	}

	const Good_kernel*
	good_parent() const {
		return dynamic_cast<const Good_kernel*>(parent());
	}


private:

	Datum _data;
	Good_kernel _carry;

};

struct Dummy_kernel: public factory::Kernel {};

template<class Carrier, class Parent=Dummy_kernel>
struct Test_kernel_stream: public test::Test<Test_kernel_stream<Carrier,Parent>> {

	typedef factory::Kernel kernel_type;
	typedef std::stringbuf sink_type;
	typedef factory::basic_kernelbuf<sys::basic_fildesbuf<char, std::char_traits<char>, sink_type>> buffer_type;
	typedef factory::Kernel_stream<kernel_type> stream_type;
	typedef typename stream_type::types types;

	void
	xrun() override {
		for (size_t i=1; i<=100; ++i) {
			do_one_iteration(i);
		}
	}

	void
	do_one_iteration(size_t count) {
		std::vector<Carrier> expected(count);
		std::vector<kernel_type*> result(count);
		buffer_type buffer{sink_type{}};
		stream_type stream(&buffer);
		test::equal(static_cast<bool>(stream), true, "bad rdstate after construction");
		// stream.settypes(&factory::types);
		for (Carrier& k : expected) {
			stream << k;
		}
		stream.flush();
		for (size_t i=0; i<count; ++i) {
			stream.sync();
			stream >> result[i];
			test::equal(static_cast<bool>(stream), true, "can not read kernel",
				"i=", i, ", rdstate=", stream.rdstate());
		}
		for (size_t i=0; i<result.size(); ++i) {
			Carrier* tmp = dynamic_cast<Carrier*>(result[i]);
			test::equal(*tmp, expected[i], "kernels are not equal", "i=", i);
		}
	}

};

int main() {
	typedef Big_kernel<100> Big_kernel_type;
	factory::register_type<Good_kernel>();
	factory::register_type<Kernel_that_writes_more_than_reads>();
	factory::register_type<Kernel_that_reads_more_than_writes>();
	factory::register_type<Dummy_kernel>();
	factory::register_type<Big_kernel_type>();
	factory::register_type({
		[] (sys::packetstream& in) {
			Kernel_that_carries_its_parent* kernel = new Kernel_that_carries_its_parent(0);
			kernel->read(in);
			return kernel;
		},
		typeid(Kernel_that_carries_its_parent)
	});
	std::clog << "Registered types:\n" << factory::types << std::flush;
	return test::Test_suite{
		new Test_kernel_stream<Good_kernel>,
		new Test_kernel_stream<Kernel_that_writes_more_than_reads>,
		new Test_kernel_stream<Kernel_that_reads_more_than_writes>,
		new Test_kernel_stream<Kernel_that_carries_its_parent, Good_kernel>,
		new Test_kernel_stream<Big_kernel_type>
	}.run();
}
