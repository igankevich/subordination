#include "factory/factory.hh"
#include <random>

using namespace factory;

template<class T>
void test_base64() {

	std::default_random_engine generator;
	std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	auto dice = std::bind(distribution, generator);

	typedef std::basic_string<T> string;

	for (size_t k=0; k<=100; ++k) {
		string text(k, 'a');
//		for (T& ch : text) ch = dice();
		string encoded = base64_encode(text.begin(), text.end());
		string decoded = base64_decode(encoded.begin(), encoded.end());
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
			test_base64<char>();
//			test_base64<unsigned char>();
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
