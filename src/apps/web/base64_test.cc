#include <gtest/gtest.h>
#include "encoding.hh"
#include "test.hh"

using factory::base64_decode;
using factory::base64_encode;
using factory::base64_encoded_size;
using factory::base64_max_decoded_size;

struct SmallSizeTest: public ::testing::TestWithParam<size_t> {};
struct BigSizeTest: public ::testing::TestWithParam<size_t> {};

std::vector<size_t> SMALL_SIZES{0, 1, 2, 3, 4, 22, 77, 4095, 4096, 4097};
std::vector<size_t> BIG_SIZES{
	std::numeric_limits<size_t>::max(),
	std::numeric_limits<size_t>::max()/4u*3u
};

TEST_P(SmallSizeTest, SmallSizes) {
	size_t k = GetParam();
	size_t sz1 = 0, sz2 = 0;
	EXPECT_NO_THROW({
		sz1 = base64_encoded_size(k);
		sz2 = base64_max_decoded_size(sz1);
	});
	EXPECT_GE(sz2, k);
}

INSTANTIATE_TEST_CASE_P(
	Base64EncodedAndMaxDecodedSize,
	SmallSizeTest,
	::testing::ValuesIn(SMALL_SIZES)
);

TEST_P(BigSizeTest, BigSizes) {
	size_t k = GetParam();
	size_t sz1 = 0, sz2 = 0;
	EXPECT_THROW({
		sz1 = base64_encoded_size(k);
		sz2 = base64_max_decoded_size(sz1);
	}, std::length_error);
	EXPECT_EQ(0, sz1);
	EXPECT_EQ(0, sz2);
}

INSTANTIATE_TEST_CASE_P(
	Base64EncodedAndMaxDecodedSize,
	BigSizeTest,
	::testing::ValuesIn(BIG_SIZES)
);

template<class T>
void test_base64(size_t k, bool spoil) {
	typedef std::basic_string<T> string;
	string text = test::random_string<T>(k);
	string encoded(base64_encoded_size(k), '_');
	string decoded(base64_max_decoded_size(encoded.size()), '_');
	base64_encode(text.begin(), text.end(), encoded.begin());
	size_t decoded_size;
	if (spoil && !encoded.empty()) {
		size_t pos = encoded.size()/2u;
		encoded[pos] = 128;
		EXPECT_THROW(
			base64_decode(encoded.begin(), encoded.end(), decoded.begin()),
			std::invalid_argument
		);
	} else {
		decoded_size = base64_decode(encoded.begin(), encoded.end(), decoded.begin());
		decoded = decoded.substr(0, decoded_size);
		EXPECT_EQ(text, decoded);
	}
}

TEST_P(SmallSizeTest, Base64EncodeDecode) {
	size_t k = GetParam();
	test_base64<char>(k, false);
	test_base64<unsigned char>(k, false);
	test_base64<char>(k, true);
	test_base64<unsigned char>(k, true);
}

INSTANTIATE_TEST_CASE_P(
	Base64EncodedDecode,
	SmallSizeTest,
	::testing::ValuesIn(SMALL_SIZES)
);

TEST_P(BigSizeTest, Base64EncodeDecode) {
	size_t k = GetParam();
	EXPECT_THROW(test_base64<char>(k, false), std::length_error);
	EXPECT_THROW(test_base64<unsigned char>(k, false), std::length_error);
	EXPECT_THROW(test_base64<char>(k, true), std::length_error);
	EXPECT_THROW(test_base64<unsigned char>(k, true), std::length_error);
}

INSTANTIATE_TEST_CASE_P(
	Base64EncodedDecode,
	BigSizeTest,
	::testing::ValuesIn(BIG_SIZES)
);
