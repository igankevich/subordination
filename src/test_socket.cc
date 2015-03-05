#include "factory/layer1.hh"
#include "factory/release_configuration.hh"
#include "factory/layer2.hh"
#include "factory/default_factory.hh"
#include "factory/layer3.hh"


#include <iostream>
#include <fstream>
#include <sstream>
#include <valarray>
#include <limits>
#include <cmath>
#include <complex>
#include <ostream>
#include <functional>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <random>

using namespace factory;

#include "datum.hh"

std::atomic<uint32_t> shutdown_counter(0);

struct Test_socket: public Mobile<Test_socket> {

	Test_socket(): _data() {}
	explicit Test_socket(std::vector<Datum> data): _data(data) {}

	void act() {
		shutdown_counter++;
		std::clog << "Test_socket::act()" << std::endl;
		std::clog << "Kernel size = " << Datum::real_size()*_data.size() << std::endl;
		commit(remote_server());
		if (shutdown_counter == 1) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			__factory.stop();
		}
	}

	void write_impl(Foreign_stream& out) {
		std::clog << "Test_socket::write_impl()" << std::endl;
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read_impl(Foreign_stream& in) {
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum> data() const { return _data; }

	static void init_type(Type* t) {
		t->id(1);
	}
	
private:
	std::vector<Datum> _data;
};

struct Main: public Identifiable<Kernel> {

	Main(): _input(VECTOR_SIZE) {}

	void act() {
		upstream(remote_server(), new Test_socket(_input));
	}

	void react(Kernel* child) {

		std::clog << "Main::react()" << std::endl;

		Test_socket* test_kernel = reinterpret_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

		if (_input.size() != output.size())
			throw std::runtime_error("test_socket. Input and output size does not match.");

		for (size_t i=0; i<_input.size(); ++i) {
			if (_input[i] != output[i]) {
				std::stringstream msg;
				msg << "test_socket. Input and output does not match: ";
				msg << _input[i] << " != " << output[i];
				throw std::runtime_error(msg.str());
			}
		}

		commit(the_server());
	}

	static const int32_t VECTOR_SIZE = 20;
	std::vector<Datum> _input;
};

struct App {
	int run(int argc, char** argv) {
		try {
			if (argc != 2)
				throw std::runtime_error("Wrong number of arguments.");
			the_server()->add(0);
			if (argv[1][0] == 'x') {
				remote_server()->socket(Endpoint("127.0.0.1", 60000));
				__factory.start();
				__factory.wait();
			}
			if (argv[1][0] == 'y') {
				remote_server()->socket(Endpoint("127.0.0.1", 60001));
				remote_server()->add(Endpoint("127.0.0.1", 60000));
				__factory.start();
				the_server()->send(new Main);
				__factory.wait();
			}
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
