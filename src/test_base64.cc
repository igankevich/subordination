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

struct App {
	int run(int, char**) {
		try {
			test_base64_size();
			test_base64<char>();
			test_base64<unsigned char>();
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
