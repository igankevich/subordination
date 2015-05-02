#include "factory/factory.hh"
#include <random>

using namespace factory;

namespace std {
	std::ostream& operator<<(std::ostream& out, const std::basic_string<unsigned char>& rhs) {
		std::ostream_iterator<char> it(out, "");
		std::copy(rhs.begin(), rhs.end(), it);
		return out;
	}
}

void test_base64_size() {
	for (size_t k=0; k<=100; ++k) {
		size_t sz1 = base64_encoded_size(k);
		size_t sz2 = base64_max_decoded_size(sz1);
		if (sz2 < k) {
			std::stringstream msg;
			msg << "Sizes do not match: ";
			msg << sz2 << " < " << k << std::endl;
			throw std::runtime_error(msg.str());
		}
	}
}

template<class T>
void test_base64() {

	std::default_random_engine generator;
	std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	auto dice = std::bind(distribution, generator);

	typedef std::basic_string<T> string;

	for (size_t k=0; k<=100; ++k) {
		string text(k, '_');
		string encoded(base64_encoded_size(k), '_');
		string decoded(base64_max_decoded_size(encoded.size()), '_');
		for (T& ch : text) ch = dice();
		base64_encode(text.begin(), text.end(), encoded.begin());
		size_t decoded_size = base64_decode(encoded.begin(), encoded.end(), decoded.begin());
		decoded = decoded.substr(0, decoded_size);
		if (decoded != text) {
			std::stringstream msg;
			msg << "Source and decoded strings do not match.\n";
			msg << text << "\n/=\n" << decoded << std::endl;
			throw std::runtime_error(msg.str());
		}
	}
}

const std::vector<std::tuple<std::string, std::string>> KNOWN_HASHES = {
	std::make_tuple("",    "da39a3ee 5e6b4b0d 3255bfef 95601890 afd80709"),
	std::make_tuple("sha", "d8f45903 20e1343a 915b6394 170650a8 f35d6926"),
	std::make_tuple("Sha", "ba79baeb 9f10896a 46ae7471 5271b7f5 86e74640"),
	std::make_tuple("The quick brown fox jumps over the lazy dog", "2fd4e1c6 7a2d28fc ed849ee1 bb76e739 1b93eb12"),
	std::make_tuple("The quick brown fox jumps over the lazy cog", "de9f2c7f d25e1b3a fad3e85a 0bd17d9b 100db4b3"),
};

void test_sha1(const std::string& input, const std::string& expected_output) {
	std::vector<uint32_t> result(5);
	sha1_encode(input.begin(), input.end(), result.begin());
	std::stringstream str;
	str << std::hex << std::setfill('0');
	std::for_each(result.begin(), result.end(), [&str] (uint32_t n) {
		str << std::setw(8) << n << ' ';
	});
	std::string output = str.str();
	output.pop_back(); // remove space character
	if (output != expected_output) {
		std::stringstream msg;
		msg << "SHA1 hash does not match the expected output:\n";
		msg << "sha1(" << input << ") = " << output;
		msg << " /= " << expected_output << std::endl;
		throw std::runtime_error(msg.str());
	}
}

void test_sha1() {
	std::for_each(KNOWN_HASHES.cbegin(), KNOWN_HASHES.cend(),
		[] (const std::tuple<std::string, std::string>& pair) {
			test_sha1(std::get<0>(pair), std::get<1>(pair));
		}
	);
}

struct App {
	int run(int, char**) {
		try {
			test_base64_size();
			test_base64<char>();
			test_base64<unsigned char>();
			test_sha1();
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
