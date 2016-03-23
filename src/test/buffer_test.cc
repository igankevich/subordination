#include <sysx/security.hh>
#include <sysx/fildesbuf.hh>
#include <sysx/packetstream.hh>
#include <factory/error.hh>
#include <factory/kernelbuf.hh>

#include "test.hh"
#include "datum.hh"

using namespace factory;

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

}

std::random_device rng;

template<class T, class Fd>
struct Test_fildesbuf: public test::Test<Test_fildesbuf<T,Fd>> {

	typedef std::char_traits<T> Tr;
	typedef sysx::basic_fdstream<T,Tr,Fd> stream_type;
	typedef Fd sink_type;

	void xrun() override {
		const size_t MAX_K = 1 << 20;
		for (size_t k=1; k<=MAX_K; k<<=1) {
			std::basic_string<T> expected_contents = test::random_string<T>(k, 'a', 'z');
			stream_type s{sink_type{}};
			s << expected_contents << std::flush;
			std::basic_stringstream<T> contents;
			contents << s.rdbuf();
			std::basic_string<T> result = contents.str();
			test::equal(result, expected_contents);
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

	typedef std::char_traits<T> Tr;
	typedef sysx::basic_fildesbuf<T,Tr,Fd> packetbuf_type;
	typedef Fd sink_type;

	void xrun() override {
		const int MAX_K = 1 << 20;
		for (int k=1; k<=MAX_K; k<<=1) {
			std::basic_string<T> contents = test::random_string<T>(k, 'a', 'z');
			packetbuf_type buf(sink_type{});
			std::basic_ostream<T> out(&buf);
			buf.begin_packet();
			out << contents;
			buf.end_packet();
			buf.sync();
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
};

template<class T, class Fd>
struct test_packetstream: public test::Test<test_packetstream<T,Fd>> {

	void xrun() override {

		typedef std::size_t I;
		typedef std::char_traits<T> Tr;
		typedef Fd sink_type;
		typedef factory::basic_kernelbuf<sysx::basic_fildesbuf<T,Tr,Fd>> basebuf;

		const I MAX_SIZE_POWER = 12;

		for (I k=0; k<=10; ++k) {

			const I size = I(1) << k;
			std::vector<Datum> input(size);
			std::vector<Datum> output(size);

			basebuf buf{sink_type{}};
			sysx::basic_packetstream<T> str(&buf);

			test::equal(str.tellp(), 0, "buffer is not empty before write");
			std::for_each(input.begin(), input.end(), [&str] (const Datum& rhs) {
				str.begin_packet();
				str << rhs;
				str.end_packet();
			});
			str.flush();

			test::equal(str.tellg(), 0, "buffer is not empty before read");

			str.fill();
			std::for_each(output.begin(), output.end(), [&str] (Datum& rhs) {
				str.read_packet();
				str >> rhs;
			});

			test::compare_bytes(input, output);
		}
	}
};

int main() {
	return test::Test_suite{
		new Test_fildesbuf<char, std::basic_stringbuf<char>>,
		new Test_fildesbuf<unsigned char, std::basic_stringbuf<unsigned char>>,
		new Test_filterbuf<char>,
		new Test_filterbuf<unsigned char>,
		new Test_packetbuf<char, std::basic_stringbuf<char>>,
		new test_packetstream<char, std::basic_stringbuf<char>>
	}.run();
}
