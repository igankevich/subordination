#include <sysx/security.hh>
#include <sysx/fildesbuf.hh>
#include <sysx/packetstream.hh>
#include <web/lbuffer.hh>
#include <factory/error.hh>
#include <factory/kernelbuf.hh>

#include <stdx/streambuf.hh>

#include "test.hh"
#include "datum.hh"

using namespace factory;
using factory::components::Error;
using sysx::basic_ofdstream;
using sysx::basic_ifdstream;
using sysx::basic_fildesbuf;
using factory::components::LBuffer;

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

}

const char* prefix = "tmp.test_buffer.";

std::random_device rng;

template<class T, template<class X> class B>
struct Test_buffer: public test::Test<Test_buffer<T,B>> {

	void xrun() override {

		stdx::adapt_engine<std::random_device, T> rng2(rng);

		typedef typename B<T>::Size I;
		const I MAX_SIZE_POWER = 12;

		for (I k=1; k<=133; ++k) {
			const I chunk_size = k;
			for (I i=0; i<=MAX_SIZE_POWER; ++i) {
				const I size = I(2) << i;
				std::vector<T> input(size);
				test::randomise(input.begin(), input.end(), rng2);
				B<T> buf(chunk_size);
				test::equal(buf.empty(), true, "buffer is not empty before write: size=", buf.size());
				buf.write(&input[0], size);
				test::equal(buf.size(), input.size(), "buffer size is not equal to input size");
				std::vector<T> output(size);
				buf.read(&output[0], size);
				test::equal(buf.empty(), true, "buffer is not empty after read: size=", buf.size());
				test::compare_bytes(input, output);
			}
		}
	}

};

template<class T, class Fd>
struct Test_fildesbuf: public test::Test<Test_fildesbuf<T,Fd>> {

	typedef std::char_traits<T> Tr;
	typedef basic_ofdstream<T,Tr,Fd> ofdstream;
	typedef basic_ifdstream<T,Tr,Fd> ifdstream;

	void xrun() override {
		const size_t MAX_K = 1 << 20;
		for (size_t k=1; k<=MAX_K; k<<=1) {
			sysx::tmpfile tfile(prefix);
			std::string filename = tfile.path();
			// fill file with random contents
			std::basic_string<T> expected_contents = test::random_string<T>(k, 'a', 'z');
			{
				sysx::file file(filename, sysx::file::write_only);
				ofdstream(std::move(file)) << expected_contents;
			}
			{
				sysx::file file(filename, sysx::file::read_only);
				ifdstream in(std::move(file));
				std::basic_stringstream<T> contents;
				contents << in.rdbuf();
				std::basic_string<T> result = contents.str();
				test::equal(result, expected_contents);
			}
			{
				sysx::file file(filename, sysx::file::write_only);
				ofdstream out(std::move(file));
				test::equal(out.eof(), false);
				out.flush();
				test::equal(out.tellp(), 0);
			}
		}
	}

};

template<class T>
struct Test_filterbuf: public test::Test<Test_filterbuf<T>> {

	void xrun() override {
		using namespace factory::components;
		const size_t MAX_K = 1 << 20;
		for (size_t k=1; k<=MAX_K; k<<=1) {
			std::basic_string<T> contents = test::random_string<T>(k, 0, 31);
			std::basic_stringstream<T> out;
			std::basic_streambuf<T>* orig = out.rdbuf();
			out.std::template basic_ostream<T>::rdbuf(new sysx::filterbuf<T>(orig));
			out << contents;
			std::basic_string<T> result = out.str();
			std::size_t cnt = std::count_if(result.begin(), result.end(), [] (T ch) {
				return ch != '\r' && ch != '\n';
			});
			if (cnt > 0) {
				std::stringstream msg;
				msg << "output stream is not properly filtered: rdbuf='"
					<< out.rdbuf() << '\'';
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}
	}
};

template<class T, class Fd=sysx::fildes>
struct Test_packetbuf: public test::Test<Test_packetbuf<T,Fd>> {

	void xrun() override {
		typedef sysx::basic_fildesbuf<T> ipacketbuf;
		typedef sysx::basic_fildesbuf<T> opacketbuf;
		const size_t MAX_K = 1 << 20;
		for (size_t k=1; k<=MAX_K; k<<=1) {
			sysx::tmpfile tfile(prefix);
			const std::string filename = tfile.path();
			std::basic_string<T> contents = test::random_string<T>(k, 'a', 'z');
			{
				sysx::file file(filename, sysx::file::write_only);
				opacketbuf buf;
				buf.setfd(std::move(file));
				std::basic_ostream<T> out(&buf);
				buf.begin_packet();
				out << contents;
				buf.end_packet();
			}
			{
				sysx::file file(filename, sysx::file::read_only);
				ipacketbuf buf;
				buf.setfd(std::move(file));
				std::basic_istream<T> in(&buf);
				std::basic_string<T> result(k, '_');
				buf.fill();
				buf.read_packet();
				in.read(&result[0], k);
				if (in.gcount() < k) {
					in.clear();
					buf.read_packet();
					in.read(&result[0], k);
				}
				test::equal(result, contents);
			}
		}
	}
};

template<class T>
struct test_packetstream: public test::Test<test_packetstream<T>> {

	void xrun() override {

		typedef std::size_t I;
		typedef std::char_traits<T> Tr;
		typedef factory::basic_kernelbuf<sysx::basic_fildesbuf<T,Tr,sysx::file>> basebuf;

		const I MAX_SIZE_POWER = 12;

		for (I k=0; k<=10; ++k) {

			sysx::tmpfile tfile(prefix);
			const std::string filename = tfile.path();

			const I size = I(1) << k;
			std::vector<Datum> input(size);
			std::vector<Datum> output(size);

			{
				sysx::file file(filename, sysx::file::write_only);
				basebuf buf(std::move(file));
				sysx::basic_packetstream<T> str(&buf);

				test::equal(str.tellp(), 0, "buffer is not empty before write");
				std::for_each(input.begin(), input.end(), [&str] (const Datum& rhs) {
					str.begin_packet();
					str << rhs;
					str.end_packet();
				});
				test::equal(str.tellg(), 0, "buffer is not empty before read");
			}

			{
				sysx::file file(filename, sysx::file::read_only);
				basebuf buf(std::move(file));
				sysx::packetstream str(&buf);

				str.fill();
				std::for_each(output.begin(), output.end(), [&str] (Datum& rhs) {
					str.read_packet();
					str >> rhs;
				});
			}

			test::compare_bytes(input, output);
		}
	}
};

int main(int argc, char* argv[]) {
	test::Test_suite tests("Test buffers");
	tests.add(new Test_buffer<char, LBuffer>);
	tests.add(new Test_buffer<unsigned char, LBuffer>);
	tests.add(new Test_fildesbuf<char, sysx::file>);
	tests.add(new Test_fildesbuf<unsigned char, sysx::file>);
	tests.add(new Test_filterbuf<char>);
	tests.add(new Test_filterbuf<unsigned char>);
	tests.add(new Test_packetbuf<char, sysx::fildes>);
	tests.add(new test_packetstream<char>);
	tests.run();
	return 0;
}
