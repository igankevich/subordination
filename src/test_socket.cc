#include "factory/factory.hh"
#include <random>

using namespace factory;

#include "datum.hh"
#include "process.hh"

const uint32_t NUM_SIZES = 13;
const uint32_t NUM_KERNELS = 7;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_KERNELS;

std::atomic<uint32_t> shutdown_counter(0);

struct Test_socket: public Mobile<Test_socket> {

	Test_socket(): _data() {}
	explicit Test_socket(std::vector<Datum> data): _data(data) {}

	void act() {
		shutdown_counter++;
		std::clog << "Test_socket::act()" << std::endl;
		std::clog << "Kernel size = " << Datum::real_size()*_data.size() << std::endl;
		commit(remote_server());
		if (shutdown_counter == TOTAL_NUM_KERNELS) {
//			std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

	Main(uint32_t n):
		_vector_size(n),
		_input(_vector_size) {}

	void act() {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			upstream(remote_server(), new Test_socket(_input));
		}
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

		std::cout << "NUM_KERNELS = " << _num_returned << std::endl;
		if (++_num_returned == NUM_KERNELS) {
			commit(the_server());
		}
	}

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
};

struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 1) {
			Process_group procs;
			procs.add([&argv] () { return execute(argv[0], 'x'); });
//			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			procs.add([&argv] () { return execute(argv[0], 'y'); });
			retval = procs.wait();
		} else {
			try {
				if (argc != 2)
					throw std::runtime_error("Wrong number of arguments.");
				the_server()->add(0);
				if (argv[1][0] == 'x') {
					the_server()->add(1);
					remote_server()->socket(Endpoint("127.0.0.1", 60000));
					__factory.start();
					__factory.wait();
				}
				if (argv[1][0] == 'y') {
					remote_server()->socket(Endpoint("127.0.0.1", 60001));
					remote_server()->add(Endpoint("127.0.0.1", 60000));
					__factory.start();
					for (uint32_t i=1; i<=NUM_SIZES; ++i)
						the_server()->send(new Main(i));
					__factory.wait();
				}
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
