#include "factory/factory.hh"
#include <random>

using namespace factory;

#include "datum.hh"

Endpoint server_endpoint("127.0.0.1", 10002);
Endpoint client_endpoint("127.0.0.2", 10002);

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
const std::vector<size_t> POWERS = {17};
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();

std::atomic<uint32_t> shutdown_counter(0);

struct Test_web_socket: public Mobile<Test_web_socket> {

	Test_web_socket(): _data() {}

	void act() {
		Logger<Level::WEBSOCKET>() << "Hello from web socket! Data = " << _data << std::endl;
		_data = 321;
		commit(ext_server());
	}

	void write_impl(Foreign_stream& out) {
		out << _data;
	}

	void read_impl(Foreign_stream& in) {
		in >> _data;
	}

	static void init_type(Type* t) {
		t->id(2);
		t->name("Test_web_socket");
	}
	
private:
	int32_t _data;
};

struct Test_socket: public Mobile<Test_socket> {

	Test_socket(): _data() {}
	explicit Test_socket(std::vector<Datum> x): _data(x) {}

	void act() {
		Logger<Level::COMPONENT> log;
		log << "kernel count = " << shutdown_counter << std::endl;
		commit(ext_server());
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

	Sender(uint32_t n, uint32_t s):
		_vector_size(n),
		_input(_vector_size),
		_sleep(s) {}

	void act() {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
			upstream(ext_server(), new Test_socket(_input));
			++shutdown_counter;
			Logger<Level::COMPONENT>() << " Sender id = " << this->id() << std::endl;
			Logger<Level::COMPONENT>() << " kernel count2 = " << shutdown_counter << std::endl;
		}
	}

	void react(Kernel* child) {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

		if (_input.size() != output.size())
			throw std::runtime_error("test_websocket. Input and output size does not match.");

		for (size_t i=0; i<_input.size(); ++i) {
			if (_input[i] != output[i]) {
				std::stringstream msg;
				msg << "test_websocket. Input and output does not match: ";
				msg << _input[i] << " != " << output[i];
				throw std::runtime_error(msg.str());
			}
		}

		Logger<Level::COMPONENT>() << "Sender::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_KERNELS) {
			commit(the_server());
		}
	}

private:

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
	uint32_t _sleep = 0;
};

struct Main: public Kernel {

	Main(uint32_t s): _sleep(s) {}

	void act() {
		for (uint32_t i=0; i<POWERS.size(); ++i) {
			size_t sz = 1 << POWERS[i];
			upstream(the_server(), new Sender(sz, _sleep));
		}
	}

	void react(Kernel*) {
		Logger<Level::COMPONENT>() << "Main::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == POWERS.size()) {
			commit(the_server());
		}
	}

private:
	uint32_t _num_returned = 0;
	uint32_t _sleep = 0;
};

uint32_t sleep_time() {
	uint32_t t = 0;
	std::stringstream s;
	s << ::getenv("SLEEP_TIME");
	s >> t;
	return t;
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 1) {
			uint32_t sleep = sleep_time();
			Process_group procs;
			procs.add([&argv, sleep] () {
				this_process::env("START_ID", 1000);
				return this_process::execute(argv[0], 'x', sleep);
			});
			// wait for master to start
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			procs.add([&argv, sleep] () {
				this_process::env("START_ID", 2000);
				return this_process::execute(argv[0], 'y', sleep);
			});
			retval = procs.wait();
		} else {
			try {
				if (argc != 3)
					throw std::runtime_error("Wrong number of arguments.");
				uint32_t sleep = 0;
				{
					std::stringstream s;
					s << argv[2];
					s >> sleep;
				}
				the_server()->add_cpu(0);
				if (argv[1][0] == 'x') {
					ext_server()->socket(server_endpoint);
					__factory.start();
					__factory.wait();
				}
				if (argv[1][0] == 'y') {
					ext_server()->socket(client_endpoint);
					ext_server()->peer(server_endpoint);
					__factory.start();
					the_server()->send(new Main(sleep));
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
