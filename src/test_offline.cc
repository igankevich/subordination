#include <factory/factory.hh>

using namespace factory;

#include "datum.hh"

Endpoint server_endpoint("127.0.0.1", 10001);
Endpoint client_endpoint("127.0.0.2", 10001);

const uint32_t NUM_SIZES = 13;
const uint32_t NUM_KERNELS = 7;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<uint32_t> shutdown_counter(0);

struct Test_socket: public Mobile<Test_socket> {

	Test_socket(): _data() {}
	explicit Test_socket(std::vector<Datum> x): _data(x) {}

	void act() {
		Logger log(Level::COMPONENT);
		log << "kernel count = " << shutdown_counter << std::endl;
		if (++shutdown_counter == TOTAL_NUM_KERNELS) {
			*(reinterpret_cast<volatile int*>(0)) = 1;
		}
//		commit(remote_server());
	}

	void write_impl(Foreign_stream& out) {
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
		t->name("Test_socket");
	}
	
private:
	std::vector<Datum> _data;
};

struct Sender: public Identifiable<Kernel> {

	Sender(uint32_t n):
		_vector_size(n),
		_input(_vector_size) {}

	void act() {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			upstream(remote_server(), new Test_socket(_input));
			++shutdown_counter;
			Logger(Level::COMPONENT) << " Sender id = " << this->id() << std::endl;
			Logger(Level::COMPONENT) << " kernel count2 = " << shutdown_counter << std::endl;
		}
	}

	void react(Kernel* child) {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		if (test_kernel->result() != Result::ENDPOINT_NOT_CONNECTED) {
			std::stringstream msg;
			msg << "Bad test kernel result: result=" << test_kernel->result();
			throw std::runtime_error(msg.str());
		}

		Logger(Level::COMPONENT) << "Sender::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_KERNELS) {
			commit(the_server());
		}
	}

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
};

struct Main: public Kernel {

	void act() {
		for (uint32_t i=1; i<=NUM_SIZES; ++i)
			upstream(the_server(), new Sender(i));
	}

	void react(Kernel*) {
		Logger(Level::COMPONENT) << "Main::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(the_server());
		}
	}

private:
	uint32_t _num_returned = 0;
};

struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 1) {
			Process_group procs;
			procs.add([&argv] () {
				this_process::env("START_ID", 1000);
				return this_process::execute(argv[0], 'x');
			});
			// wait for master to start
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			procs.add([&argv] () {
				this_process::env("START_ID", 2000);
				return this_process::execute(argv[0], 'y');
			});
			int ret = procs.wait();
			retval = ret == SIGSEGV ? 0 : ret;
		} else {
			try {
				if (argc != 2)
					throw std::runtime_error("Wrong number of arguments.");
				the_server()->add_cpu(0);
				if (argv[1][0] == 'x') {
					the_server()->add_cpu(1);
					remote_server()->socket(server_endpoint);
					__factory.start();
					__factory.wait();
				}
				if (argv[1][0] == 'y') {
					remote_server()->socket(client_endpoint);
					remote_server()->peer(server_endpoint);
					__factory.start();
					the_server()->send(new Main);
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
