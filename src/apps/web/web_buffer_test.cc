#include "lbuffer.hh"
#include <factory/error.hh>
#include <sys/bits/buffer_category.hh>
#include "websocket.hh"

#include "test.hh"
#include "datum.hh"

using namespace factory;
using factory::components::Error;
using factory::components::LBuffer;

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sys::buffer_category>:
	public std::integral_constant<bool, true> {};

}

const char* prefix = "tmp.web_buffer_test.";

std::random_device rng;

template<class T, template<class X> class B>
struct Test_buffer: public test::Parametric_test<Test_buffer<T,B>,std::array<size_t,2>> {

	typedef test::Parametric_test<Test_buffer<T,B>,std::array<size_t,2>> base_type;
	using typename base_type::array_type;
	using typename base_type::size_type;

	Test_buffer(array_type&& minval, array_type&& maxval):
	base_type(std::forward<array_type>(minval), std::forward<array_type>(maxval))
	{}

	void parametric_run(size_t chunk_size, size_t i) override {
		stdx::adapt_engine<std::random_device, T> rng2(rng);
		const size_type size = size_type(2) << i;
		std::vector<T> input(size);
		test::randomise(input.begin(), input.end(), rng2);
		B<T> buf(chunk_size);
		test::equal(buf.empty(), true, "buffer is not empty before write", "size=", buf.size());
		buf.write(&input[0], size);
		test::equal(buf.size(), input.size(), "buffer size is not equal to input size");
		std::vector<T> output(size);
		buf.read(&output[0], size);
		test::equal(buf.empty(), true, "buffer is not empty after read", "size=", buf.size());
		test::compare_bytes(input, output, "input does not match output");
	}

};

int main() {
	return test::Test_suite{
		new Test_buffer<char, LBuffer>{{1,0}, {133, 12}},
		new Test_buffer<unsigned char, LBuffer>{{1,0}, {133, 12}}
	}.run();
}
