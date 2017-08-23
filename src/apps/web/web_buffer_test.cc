#include "lbuffer.hh"
#include <factory/error.hh>
#include "websocket.hh"

#include <unistdx/base/adapt_engine>
#include <gtest/gtest.h>

#include "test.hh"
#include "datum.hh"
#include "make_types.hh"

using namespace factory;
using factory::Error;
using factory::LBuffer;

const char* prefix = "tmp.web_buffer_test.";

std::random_device rng;


typedef std::pair<size_t,size_t> param_type;

struct LBufferTest: public ::testing::TestWithParam<param_type> {};

std::vector<param_type> PARAMS{{1,0}, {133, 12}};

template <class T>
void
test_lbuffer(size_t chunk_size, size_t power) {
	sys::adapt_engine<std::random_device, T> rng2(rng);
	const size_t size = size_t(2) << power;
	std::vector<T> input(size);
	test::randomise(input.begin(), input.end(), rng2);
	LBuffer<T> buf(chunk_size);
	EXPECT_TRUE(buf.empty())
		<< "buffer is not empty before write: size="
		<< buf.size();
	buf.write(&input[0], size);
	EXPECT_EQ(input.size(), buf.size());
	std::vector<T> output(size);
	buf.read(&output[0], size);
	EXPECT_TRUE(buf.empty())
		<< "buffer is not empty after read: size="
		<<  buf.size();
	EXPECT_EQ(input, output);
}

TEST_P(LBufferTest, IO) {
	size_t chunk_size = GetParam().first;
	size_t power = GetParam().second;
	test_lbuffer<char>(chunk_size, power);
	test_lbuffer<unsigned char>(chunk_size, power);
}

INSTANTIATE_TEST_CASE_P(
	LBufferTestAllSizes,
	LBufferTest,
	::testing::ValuesIn(PARAMS)
);

