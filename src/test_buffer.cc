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

const char* prefix = "tmp.test_buffer.";

std::random_device rng;

template<class T, template<class X> class B>
void test_buffer() {

	stdx::adapt_engine<std::random_device, T> rng2(rng);

	typedef typename B<T>::Size I;
	const I MAX_SIZE_POWER = 12;

	for (I k=1; k<=133; ++k) {
		const I chunk_size = k;
		for (I i=0; i<=MAX_SIZE_POWER; ++i) {
			I size = I(2) << i;
			std::vector<T> input(size);
			test::randomise(input.begin(), input.end(), rng2);
			B<T> buf(chunk_size);
			if (!buf.empty()) {
				std::stringstream msg;
				msg << "buffer is not empty before write: size=";
				msg << buf.size();
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
			buf.write(&input[0], size);
			if (buf.size() != input.size()) {
				std::stringstream msg;
				msg << "buffer size is not equal to input size: ";
				msg << buf.size() << " != " << input.size();
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
			std::vector<T> output(size);
			buf.read(&output[0], size);
			if (!buf.empty()) {
				std::stringstream msg;
				msg << "buffer is not empty after read: size=";
				msg << buf.size();
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
			for (size_t j=0; j<size; ++j) {
				if (input[j] != output[j]) {
					std::stringstream msg;
					msg << "input and output does not match:\n'"
						<< input[j] << "'\n!=\n'" << output[j] << "'";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
			}
		}
	}
}

template<class T, class Fd>
void test_fildesbuf() {
	typedef std::char_traits<T> Tr;
	typedef basic_ofdstream<T,Tr,Fd> ofdstream;
	typedef basic_ifdstream<T,Tr,Fd> ifdstream;
	const size_t MAX_K = 1 << 20;
	for (size_t k=1; k<=MAX_K; k<<=1) {
		sysx::tmpfile tfile(prefix);
		std::string filename = tfile.path();
		// fill file with random contents
		std::basic_string<T> expected_contents = test::random_string<T>(k, 'a', 'z');
		{
			std::clog << "Checking overflow()" << std::endl;
			sysx::file file(filename, sysx::file::write_only);
			ofdstream(std::move(file)) << expected_contents;
		}
		{
			std::clog << "Checking underflow()" << std::endl;
			sysx::file file(filename, sysx::file::read_only);
			ifdstream in(std::move(file));
			std::basic_stringstream<T> contents;
			contents << in.rdbuf();
			std::basic_string<T> result = contents.str();
			if (result != expected_contents) {
				std::stringstream msg;
				msg << "input and output does not match:\n'"
					<< expected_contents << "'\n!=\n'" << result << "'";
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}
		{
			std::clog << "Checking flush()" << std::endl;
			sysx::file file(filename, sysx::file::write_only);
			ofdstream out(std::move(file));
			test::equal(out.eof(), false);
			out.flush();
			test::equal(out.tellp(), 0);
		}
	}
}

template<class T>
void test_filterbuf() {
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

template<class T, class Fd=sysx::fildes>
void test_packetbuf() {
	std::clog << "Checking packetbuf" << std::endl;
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
//			std::clog << "Result: "
//				<< "tellg=" << in.tellg()
//				<< ", size=" << result.size()
//				<< ", result='" << Binary<std::basic_string<T>>(result) << '\'' << std::endl;
			if (result != contents) {
				std::stringstream msg;
				msg << '[' << __func__ << ']';
				msg << "input and output does not match:\n'"
					<< contents << "'\n!=\n'" << result << "'";
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}
	}
}

template<class T>
void test_packetstream() {

	typedef std::size_t I;
	typedef std::char_traits<T> Tr;
	typedef factory::basic_kernelbuf<sysx::basic_fildesbuf<T,Tr,sysx::file>> basebuf;

	const I MAX_SIZE_POWER = 12;
	std::clog << "test_packetstream()" << std::endl;

	for (I k=0; k<=10; ++k) {

		sysx::tmpfile tfile(prefix);
		const std::string filename = tfile.path();

		const I size = I(1) << k;
		std::vector<Datum> input(size);
		std::vector<Datum> output(size);

		{
			sysx::file file(filename, sysx::file::write_only);
			basebuf buf(std::move(file));
			sysx::packetstream str(&buf);

			if (str.tellp() != 0) {
				std::stringstream msg;
				msg << "input buffer is not empty before write: tellp=";
				msg << str.tellp();
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
			std::for_each(input.begin(), input.end(), [&str] (const Datum& rhs) {
				str.begin_packet();
				str << rhs;
				str.end_packet();
			});
			if (str.tellg() != 0) {
				std::stringstream msg;
				msg << "output buffer is not empty before read: tellg=";
				msg << str.tellg();
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
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

		test::compare(input, output);
//		auto pair = std::mismatch(input.begin(), input.end(), output.begin());
//		if (pair.first != input.end()) {
//			auto pos = pair.first - input.begin();
//			std::stringstream msg;
//			msg << '[' << __func__ << ']';
//			msg << " input and output does not match at i=" << pos << ":\n'"
//				<< sysx::make_bytes(*pair.first) << "'\n!=\n'" << sysx::make_bytes(*pair.second) << "'";
//			throw Error(msg.str(), __FILE__, __LINE__, __func__);
//		}
	}
}

int main(int argc, char* argv[]) {
	test_buffer<char, LBuffer>();
	test_buffer<unsigned char, LBuffer>();
	test_fildesbuf<char, sysx::file>();
//	test_fildesbuf<unsigned char, int>();
//	test_fildesbuf<char, Socket>();
	test_filterbuf<char>();
	test_packetbuf<char>();
	test_packetstream<char>();
	return 0;
}
