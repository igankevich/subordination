#include <sysx/network_format.hh>
#include <sysx/fildesbuf.hh>

#include <factory/factory.hh>
#include <factory/server/basic_server.hh>
#include <factory/kernel.hh>
#include <factory/kernelbuf.hh>
#include <factory/kernel_stream.hh>

#include "test.hh"
#include "datum.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

}

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::No_server<config> local_server;
			typedef components::No_server<config> remote_server;
			typedef components::No_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;

#include "big_kernel.hh"

struct Good_kernel: public Kernel {

	void
	write(sysx::packetstream& out) override {
		Kernel::write(out);
		out << _data;
	}

	void
	read(sysx::packetstream& in) override {
		Kernel::read(in);
		in >> _data;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			1,
			"Good_kernel",
			[] (sysx::packetstream& in) {
				Good_kernel* k = new Good_kernel;
				k->read(in);
				return k;
			}
		};
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
		return out << sysx::make_bytes(rhs._data);
	}


private:

	Datum _data;

};

struct Kernel_that_writes_more_than_reads: public Good_kernel {

	void
	write(sysx::packetstream& out) override {
		Good_kernel::write(out);
		// push dummy object to the stream
		out << Datum();
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			2,
			"Kernel_that_writes_more_than_reads",
			[] (sysx::packetstream& in) {
				Kernel_that_writes_more_than_reads* k = new Kernel_that_writes_more_than_reads;
				k->read(in);
				return k;
			}
		};
	}

};

struct Kernel_that_reads_more_than_writes: public Good_kernel {

	void
	read(sysx::packetstream& in) override {
		Good_kernel::read(in);
		Datum dummy;
		// read dummy object from the stream
		in >> dummy;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			3,
			"Kernel_that_reads_more_than_writes",
			[] (sysx::packetstream& in) {
				Kernel_that_reads_more_than_writes* k = new Kernel_that_reads_more_than_writes;
				k->read(in);
				return k;
			}
		};
	}

};

template<class Kernel, class Carrier, class Ch=char>
struct Test_kernel_stream: public test::Test<Test_kernel_stream<Kernel,Carrier,Ch>> {

	typedef Kernel kernel_type;
	typedef basic_kernelbuf<sysx::basic_fildesbuf<Ch, std::char_traits<Ch>, sysx::file>> buffer_type;
	typedef Kernel_stream<kernel_type> stream_type;
	typedef typename stream_type::types types;
	typedef stdx::log<Test_kernel_stream> this_log;

	Test_kernel_stream() {
		_types.register_type(Carrier::static_type());
	}

	void
	xrun() override {
		for (size_t i=1; i<=100; ++i) {
			do_one_iteration(i);
		}
	}

	void
	do_one_iteration(size_t count) {
		sysx::tmpfile tfile(prefix);
		const std::string filename = tfile.path();
		std::vector<Carrier> expected(count);
		std::vector<kernel_type*> result(count);
		{
			buffer_type buffer;
			sysx::file file(filename, sysx::file::write_only);
			buffer.setfd(std::move(file));
			stream_type stream(&buffer);
			test::equal(static_cast<bool>(stream), true, "bad rdstate after construction");
			stream.settypes(&_types);
			for (Carrier& k : expected) {
				stream << k;
			}
			stream.flush();
		}
		{
			buffer_type buffer;
			sysx::file file(filename, sysx::file::read_only);
			buffer.setfd(std::move(file));
			stream_type stream(&buffer);
			test::equal(static_cast<bool>(stream), true, "bad rdstate after construction");
			stream.settypes(&_types);
			for (size_t i=0; i<count; ++i) {
				stream.fill();
				stream >> result[i];
				test::equal(static_cast<bool>(stream), true, "can not read kernel",
					"i=", i, ", rdstate=", stream.rdstate());
			}
		}
		for (size_t i=0; i<result.size(); ++i) {
			Carrier* tmp = dynamic_cast<Carrier*>(result[i]);
			test::equal(*tmp, expected[i], "kernels are not equal", "i=", i);
		}
	}

private:

	types _types;

	static constexpr const char* prefix = "tmp.test_buffer.";

};

int main() {
	test::Test_suite tests{"Test buffers", {
		new Test_kernel_stream<Kernel, Good_kernel>,
		new Test_kernel_stream<Kernel, Kernel_that_writes_more_than_reads>,
		new Test_kernel_stream<Kernel, Kernel_that_reads_more_than_writes>,
		new Test_kernel_stream<Kernel, Big_kernel<100>>
	}};
	return tests.run();
}
