#include <factory/factory.hh>
#include "test.hh"

using namespace factory;

template<class T, template<class X> class B>
void test_buffer() {

	std::default_random_engine generator;
	std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	auto dice = std::bind(distribution, generator);

	typedef typename B<T>::Size I;
	const I MAX_SIZE_POWER = 12;

	for (I k=1; k<=133; ++k) {
		const I chunk_size = k;
		for (I i=0; i<=MAX_SIZE_POWER; ++i) {
			I size = I(2) << i;
			std::vector<T> input(size);
			for (size_t j=0; j<size; ++j)
				input[j] = dice();
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

template<class T>
void test_socket_buf() {
	std::string filename = "/tmp/"
		+ test::random_string<T>(16, 'a', 'z')
		+ ".factory";
	const size_t MAX_K = 1 << 20;
	for (size_t k=1; k<=MAX_K; k<<=1) {
		// fill file with random contents
		std::string expected_contents = test::random_string<T>(k, 'a', 'z');
//		std::ofstream(filename) << expected_contents;
		int fd = check("open()", ::open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR));
		factory::fd_ostream(fd) << expected_contents;
		check("close()", ::close(fd));
		fd = check("open()", ::open(filename.c_str(), O_RDONLY));
		factory::fd_istream in(fd);
		test::equal(in.tellg(), 0);
		std::stringstream contents;
		contents << in.rdbuf();
		std::string result = contents.str();
//		test::equal(in.tellg(), result.size());
		if (result != expected_contents) {
			std::stringstream msg;
			msg << "input and output does not match:\n'"
				<< expected_contents << "'\n!=\n'" << result << "'";
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		check("close()", ::close(fd));
	}
	check("remove()", std::remove(filename.c_str()));
}

struct App {
	int run(int, char**) {
		try {
			test_buffer<char, Buffer>();
			test_buffer<unsigned char, Buffer>();
			test_buffer<char, LBuffer>();
			test_buffer<unsigned char, LBuffer>();
			test_socket_buf<char>();
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
