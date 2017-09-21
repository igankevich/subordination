#include <unistdx/io/fildesbuf>

#include <bscheduler/kernel/kstream.hh>
#include <bscheduler/ppl/basic_pipeline.hh>

#include "datum.hh"
#include "big_kernel.hh"
#include "make_types.hh"

#include <gtest/gtest.h>

struct Good_kernel: public bsc::kernel {

	void
	write(sys::pstream& out) override {
		bsc::kernel::write(out);
		out << _data;
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
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
	write(sys::pstream& out) override {
		Good_kernel::write(out);
		// push dummy object to the stream
		out << Datum();
	}

};

struct Kernel_that_reads_more_than_writes: public Good_kernel {

	void
	read(sys::pstream& in) override {
		Good_kernel::read(in);
		Datum dummy;
		// read dummy object from the stream
		in >> dummy;
	}

};

struct Kernel_that_carries_its_parent: public bsc::kernel {

	Kernel_that_carries_its_parent() {
		this->setf(bsc::kernel_flag::carries_parent);
		this->parent(&_carry);
		assert(this->parent());
	}

	Kernel_that_carries_its_parent(int) {}

	void
	write(sys::pstream& out) override {
		bsc::kernel::write(out);
		out << _data;
	}

	void
	read(sys::pstream& in) override {
		bsc::kernel::read(in);
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

struct Dummy_kernel: public bsc::kernel {};

typedef Big_kernel<100> Big_kernel_type;

bool registered = false;

void
register_all() {
	if (!registered) {
		bsc::register_type<Good_kernel>();
		bsc::register_type<Kernel_that_writes_more_than_reads>();
		bsc::register_type<Kernel_that_reads_more_than_writes>();
		bsc::register_type<Dummy_kernel>();
		bsc::register_type<Big_kernel_type>();
		bsc::register_type({
			[] (sys::pstream& in) {
				Kernel_that_carries_its_parent* k = new Kernel_that_carries_its_parent(0);
				k->read(in);
				return k;
			},
			typeid(Kernel_that_carries_its_parent)
		});
		std::clog << "Registered types:\n" << bsc::types << std::flush;
		registered = true;
	}
}

template<class Pair>
struct KernelStreamTest: public ::testing::Test {
	KernelStreamTest() {
		register_all();
	}
};

TYPED_TEST_CASE(
	KernelStreamTest,
	MAKE_TYPES(
		Good_kernel,
		Kernel_that_writes_more_than_reads,
		Kernel_that_reads_more_than_writes,
		Kernel_that_carries_its_parent,
		Big_kernel_type
	)
);

TYPED_TEST(KernelStreamTest, IO) {
	typedef TypeParam Carrier;
	typedef bsc::kernel kernel_type;
	typedef std::stringbuf sink_type;
	typedef sys::basic_fildesbuf<char, std::char_traits<char>, sink_type>
		fildesbuf_type;
	typedef bsc::basic_kernelbuf<fildesbuf_type> buffer_type;
	typedef bsc::kstream<kernel_type> stream_type;
	for (size_t count=1; count<=100; ++count) {
		std::vector<Carrier> expected(count);
		std::vector<kernel_type*> result(count);
		buffer_type buffer;
		buffer.setfd(sink_type{});
		stream_type stream(&buffer);
		EXPECT_TRUE(static_cast<bool>(stream));
		for (Carrier& k : expected) {
			stream.begin_packet();
			stream << k;
			stream.end_packet();
		}
		for (size_t i=0; i<count; ++i) {
			stream.sync();
			stream.read_packet();
			stream >> result[i];
			stream.skip_packet();
		}
		for (size_t i=0; i<result.size(); ++i) {
			Carrier* tmp = dynamic_cast<Carrier*>(result[i]);
			ASSERT_EQ(expected[i], *tmp) << "i=" << i
				<< ",type=" << typeid(Carrier).name();
			if (tmp->parent()) {
				delete tmp->parent();
			}
			delete tmp;
		}
	}
}
