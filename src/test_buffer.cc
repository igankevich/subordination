#include <sysx/security.hh>
#include <factory/ext/fdbuf.hh>
#include <factory/ext/kernelbuf.hh>
#include <factory/ext/lbuffer.hh>
#include <factory/error.hh>

#include <stdx/streambuf.hh>

#include "test.hh"
#include "datum.hh"

using namespace factory;
using factory::components::Error;
using factory::components::basic_ofdstream;
using factory::components::basic_ifdstream;
using factory::components::basic_okernelbuf;
using factory::components::basic_ikernelbuf;
using factory::components::basic_kernelbuf;
using factory::components::basic_fdbuf;
using factory::components::basic_kstream;
using factory::components::LBuffer;
using factory::components::end_packet;

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
void test_fdbuf() {
	typedef std::char_traits<T> Tr;
	std::string filename = "/tmp/"
		+ test::random_string<char>(16, 'a', 'z')
		+ ".factory";
	const size_t MAX_K = 1 << 20;
	for (size_t k=1; k<=MAX_K; k<<=1) {
		sysx::tmpfile tfile(filename);
		// fill file with random contents
		std::basic_string<T> expected_contents = test::random_string<T>(k, 'a', 'z');
		{
			std::clog << "Checking overflow()" << std::endl;
			sysx::file file(filename, sysx::file::write_only);
			basic_ofdstream<T,Tr,Fd>(std::move(file)) << expected_contents;
		}
		{
			std::clog << "Checking underflow()" << std::endl;
			sysx::file file(filename, sysx::file::read_only);
			basic_ifdstream<T,Tr,Fd> in(std::move(file));
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
			basic_ofdstream<T,Tr,Fd> out(std::move(file));
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

template<class T, class Fd=sysx::fd>
void test_kernelbuf() {
	std::clog << "Checking kernelbuf" << std::endl;
	std::string filename = "/tmp/"
		+ test::random_string<char>(16, 'a', 'z')
		+ ".factory";
	typedef basic_ikernelbuf<basic_fdbuf<T,Fd>> ikernelbuf;
	typedef basic_okernelbuf<basic_fdbuf<T,Fd>> okernelbuf;
	const size_t MAX_K = 1 << 20;
	for (size_t k=1; k<=MAX_K; k<<=1) {
		sysx::tmpfile tfile(filename);
		std::basic_string<T> contents = test::random_string<T>(k, 'a', 'z');
		{
			sysx::file file(filename, sysx::file::write_only);
			okernelbuf buf;
			buf.setfd(std::move(file));
			std::basic_ostream<T> out(&buf);
			out << contents;
		}
		{
			sysx::file file(filename, sysx::file::read_only);
			ikernelbuf buf;
			buf.setfd(std::move(file));
			std::basic_istream<T> in(&buf);
			std::basic_string<T> result(k, '_');
			in.read(&result[0], k);
			if (in.gcount() < k) {
				in.clear();
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
void test_kernelbuf_with_stringstream() {
	typedef basic_kernelbuf<std::basic_stringbuf<T>> kernelbuf;
	const size_t MAX_K = 1 << 20;
	for (size_t k=1; k<=MAX_K; k<<=1) {
		std::basic_string<T> contents = test::random_string<T>(k, 'a', 'z');
		std::basic_stringstream<T> out;
		std::basic_streambuf<T>* orig = out.rdbuf();
		kernelbuf buf;
		static_cast<std::basic_ostream<T>&>(out).rdbuf(&buf);
		out.write(contents.data(), contents.size());
		out << end_packet() << std::flush;
		std::basic_string<T> result(contents.size(), '_');
		out.read(&result[0], result.size());
		if (result != contents) {
			std::stringstream msg;
			msg << '[' << __func__ << ']';
			msg << "input and output does not match:\n'"
				<< contents << "'\n!=\n'" << result << "'";
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		static_cast<std::basic_ostream<T>&>(out).rdbuf(orig);
	}
}

template<class T>
void test_kernelbuf_withvector() {

	typedef std::size_t I;
	typedef basic_kernelbuf<std::basic_stringbuf<T>> kernelbuf;
	typedef basic_kstream<std::basic_stringbuf<T>> kstream;

	const I MAX_SIZE_POWER = 12;
	std::clog << "test_kernelbuf_withvector()" << std::endl;

	for (I k=0; k<=10; ++k) {
		I size = I(1) << k;
		std::vector<Datum> input(size);
		kstream str;
		if (str.tellp() != 0) {
			std::stringstream msg;
			msg << "input buffer is not empty before write: tellp=";
			msg << str.tellp();
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		std::for_each(input.begin(), input.end(), [&str] (const Datum& rhs) {
			str << rhs << end_packet();
		});
		if (str.tellg() != 0) {
			std::stringstream msg;
			msg << "output buffer is not empty before read: tellg=";
			msg << str.tellg();
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		std::vector<Datum> output(size);
		std::for_each(output.begin(), output.end(), [&str] (Datum& rhs) {
			str >> rhs;
		});
		if (str.tellg() != str.tellp()) {
			std::stringstream msg;
			msg << "read and write positions does not match after read: tellp=";
			msg << str.tellp();
			msg << ", tellg=" << str.tellg();
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		auto pair = std::mismatch(input.begin(), input.end(), output.begin());
		if (pair.first != input.end()) {
			auto pos = pair.first - input.begin();
			std::stringstream msg;
			msg << '[' << __func__ << ']';
			msg << "input and output does not match at i=" << pos << ":\n'"
				<< sysx::make_bytes(*pair.first) << "'\n!=\n'" << sysx::make_bytes(*pair.second) << "'";
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
	}
}

struct Stream_source {

	explicit
	Stream_source(std::istream& s):
	_stream(s)
	{}

	ssize_t
	sgetn(char* buf, size_t n) const noexcept {
		_stream.read(static_cast<char*>(buf), n);
		return _stream.gcount();
	}

private:
	std::istream& _stream;
};

struct Stream_sink {

	explicit
	Stream_sink(std::ostream& s):
	_stream(s)
	{}

	ssize_t
	sputn(const char* buf, size_t n) const noexcept {
		const std::streampos p0 = _stream.tellp();
		_stream.write(buf, n);
		const std::streampos p1 = _stream.tellp();
		return p1-p0;
	}

private:
	std::ostream& _stream;
};

int main(int argc, char* argv[]) {
	test_buffer<char, LBuffer>();
	test_buffer<unsigned char, LBuffer>();
	test_fdbuf<char, sysx::file>();
//	test_fdbuf<unsigned char, int>();
//	test_fdbuf<char, Socket>();
	test_filterbuf<char>();
//	test_filterbuf<unsigned char>();
	test_kernelbuf<char>();
//	test_kernelbuf_with_stringstream<char>();
	test_kernelbuf_withvector<char>();
	return 0;
}
