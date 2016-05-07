#include <sys/net/network_format.hh>
#include <sys/io/fildesbuf.hh>

#include <factory/kernel.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>
#include <factory/reflection.hh>

#include "test.hh"
#include "datum.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sys::buffer_category>:
	public std::integral_constant<bool, true> {};

}

struct Dummy_remote_server {
	template<class T>
	void send(T*) {}
} remote_server;


#include "big_kernel.hh"

using namespace factory;

struct Good_kernel: public Kernel, public Register_type<Good_kernel> {

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << _data;
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
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
//		operator<<(out, static_cast<const Kernel&>(rhs));
		return out << sys::make_bytes(rhs._data);
	}

	const Datum&
	data() const noexcept {
		return _data;
	}

private:

	Datum _data;

};

struct Kernel_that_writes_more_than_reads:
public Good_kernel,
public Register_type<Kernel_that_writes_more_than_reads>
{

	void
	write(sys::packetstream& out) override {
		Good_kernel::write(out);
		// push dummy object to the stream
		out << Datum();
	}

};

struct Kernel_that_reads_more_than_writes:
public Good_kernel,
public Register_type<Kernel_that_reads_more_than_writes>
{

	void
	read(sys::packetstream& in) override {
		Good_kernel::read(in);
		Datum dummy;
		// read dummy object from the stream
		in >> dummy;
	}

};

struct Kernel_that_carries_its_parent: public Kernel {

	Kernel_that_carries_its_parent() {
		this->setf(Kernel::Flag::carries_parent);
		this->parent(&_carry);
		assert(parent());
	}

	Kernel_that_carries_its_parent(int) {}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << _data;
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
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

struct Dummy_kernel: public Kernel, public Register_type<Dummy_kernel> {
};

template<class Kernel, class Carrier, class Parent=Dummy_kernel, class Ch=char>
struct Test_kernel_stream: public test::Test<Test_kernel_stream<Kernel,Carrier,Parent,Ch>> {

	typedef Kernel kernel_type;
	typedef std::basic_stringbuf<Ch> sink_type;
	typedef basic_kernelbuf<sys::basic_fildesbuf<Ch, std::char_traits<Ch>, sink_type>> buffer_type;
	typedef Kernel_stream<kernel_type> stream_type;
	typedef typename stream_type::types types;
	typedef stdx::log<Test_kernel_stream> this_log;

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
	std::clog << factory::types << std::endl;
	factory::register_type({
		[] (sys::packetstream& in) {
			Kernel_that_carries_its_parent* kernel = new Kernel_that_carries_its_parent(0);
			kernel->read(in);
			return kernel;
		},
		typeid(Kernel_that_carries_its_parent)
	});
	return test::Test_suite{
		new Test_kernel_stream<Kernel, Good_kernel>,
		new Test_kernel_stream<Kernel, Kernel_that_writes_more_than_reads>,
		new Test_kernel_stream<Kernel, Kernel_that_reads_more_than_writes>,
		new Test_kernel_stream<Kernel, Kernel_that_carries_its_parent, Good_kernel>,
		new Test_kernel_stream<Kernel, Big_kernel<100>>
	}.run();
}
