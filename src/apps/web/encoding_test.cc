#include <algorithm>
#include <gtest/gtest.h>
#include <iomanip>
#include <string>
#include <vector>

#include "encoding.hh"
#include "test.hh"

using namespace factory;

std::random_device rng;

TEST(WebSocket, EncodeDecode) {
	std::vector<std::string> inputs = {""};
	for (int i=0; i<20; ++i) {
		std::string str = test::random_string<char>(1 << i);
		inputs.push_back(str);
	}
	std::for_each(inputs.begin(), inputs.end(),
		[] (const std::string& in) {
			std::stringstream str;
			std::ostream_iterator<char> oit(str);
			std::stringstream str2;
			std::ostream_iterator<char> oit2(str2);
			Opcode opcode;
			websocket_encode(in.begin(), in.end(), oit, rng);
			std::string encoded = str.str();
			websocket_decode(encoded.begin(), encoded.end(), oit2, &opcode);
			std::string out = str2.str();
			EXPECT_EQ(out, in);
		}
	);
}
