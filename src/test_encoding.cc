#include <factory/factory_base.hh>
#include "test.hh"

using namespace factory;

void test_base64_size() {
	for (size_t k=0; k<=100; ++k) {
		size_t sz1 = base64_encoded_size(k);
		size_t sz2 = base64_max_decoded_size(sz1);
		if (sz2 < k) {
			std::stringstream msg;
			msg << "Sizes do not match: ";
			msg << sz2 << " < " << k << std::endl;
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
	}
}

template<class T>
void test_base64(bool spoil=false) {

	typedef std::basic_string<T> string;

	for (size_t k=0; k<=100; ++k) {
		string text = test::random_string<T>(k);
		string encoded(base64_encoded_size(k), '_');
		string decoded(base64_max_decoded_size(encoded.size()), '_');
		base64_encode(text.begin(), text.end(), encoded.begin());
		size_t decoded_size;
		bool ok = false;
		if (spoil && !encoded.empty()) {
			size_t pos = encoded.size()/2u;
			encoded[pos] = 128;
			try {
				decoded_size = base64_decode(encoded.begin(), encoded.end(), decoded.begin());
			} catch (std::invalid_argument& err) {
				ok = true;
			}
		} else {
			decoded_size = base64_decode(encoded.begin(), encoded.end(), decoded.begin());
			ok = true;
			decoded = decoded.substr(0, decoded_size);
			if (decoded != text) {
				std::stringstream msg;
				msg << "source and decoded strings do not match:\n";
				msg << text << "\n/=\n" << decoded << std::endl;
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}
		if (!ok) {
			throw Error("invalid string was decoded by decode_base64()",
				__FILE__, __LINE__, __func__);
		}
	}
}

std::string sha1_digest_to_string(const std::vector<uint32_t>& result) {
	std::stringstream str;
	str << std::hex << std::setfill('0');
	std::for_each(result.begin(), result.end(), [&str] (uint32_t n) {
		str << std::setw(8) << n << ' ';
	});
	std::string output = str.str();
	output.pop_back(); // remove space character
	return output;
}

void validate_sha1_output(const std::string& input, const std::string& output, const std::string& expected_output) {
	if (output != expected_output) {
		std::stringstream msg;
		msg << "SHA1 hash does not match the expected output:\n";
		msg << "sha1(" << input << ") = " << output;
		msg << " /= " << expected_output << std::endl;
		throw Error(msg.str(), __FILE__, __LINE__, __func__);
	}
}

void test_sha1(const std::string& input, const std::string& expected_output) {
	std::vector<uint32_t> result(5);
	sha1_encode(input.begin(), input.end(), result.begin());
	std::string output = sha1_digest_to_string(result);
	validate_sha1_output(input, output, expected_output);
}

void test_sha1_big_input() {
	static const std::string expected_output = "34aa973c d4c4daa4 f61eeb2b dbad2731 6534016f";
	SHA1 sha1;
	std::string a = "aaaaaaaaaa";
    for(int i=0; i<100000; i++)
		sha1.input(a.begin(), a.end());
	std::vector<uint32_t> result(5);
	sha1.result(result.begin());
	std::string output = sha1_digest_to_string(result);
	validate_sha1_output("one million of 'a'", output, expected_output);
}

void test_sha1() {
	const std::vector<std::tuple<std::string, std::string>> KNOWN_HASHES = {
		std::make_tuple("",    "da39a3ee 5e6b4b0d 3255bfef 95601890 afd80709"),
		std::make_tuple("sha", "d8f45903 20e1343a 915b6394 170650a8 f35d6926"),
		std::make_tuple("Sha", "ba79baeb 9f10896a 46ae7471 5271b7f5 86e74640"),
		std::make_tuple("The quick brown fox jumps over the lazy dog", "2fd4e1c6 7a2d28fc ed849ee1 bb76e739 1b93eb12"),
		std::make_tuple("The quick brown fox jumps over the lazy cog", "de9f2c7f d25e1b3a fad3e85a 0bd17d9b 100db4b3"),
		std::make_tuple("abc", "a9993e36 4706816a ba3e2571 7850c26c 9cd0d89d"),
		std::make_tuple("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e44 1c3bd26e baae4aa1 f95129e5 e54670f1"),
	};
	std::for_each(KNOWN_HASHES.cbegin(), KNOWN_HASHES.cend(),
		[] (const std::tuple<std::string, std::string>& pair) {
			test_sha1(std::get<0>(pair), std::get<1>(pair));
		}
	);
}

void test_web_socket() {
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
			websocket_encode(in.begin(), in.end(), oit);
			std::string encoded = str.str();
			websocket_decode(encoded.begin(), encoded.end(), oit2, &opcode);
			std::string out = str2.str();
			if (in != out) {
				std::stringstream msg;
				msg << "input and output does not match for size="
					<< in.size() << ":\n" << in << "\n!=\n" << out;
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}
	);
}

struct App {
	int run(int, char**) {
		try {
			test_base64_size();
			test_base64<char>();
			test_base64<unsigned char>();
			test_base64<char>(true);
			test_base64<unsigned char>(true);
			test_sha1();
			test_sha1_big_input();
			test_web_socket();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
