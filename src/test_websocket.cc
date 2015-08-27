#include <factory/factory.hh>

using namespace factory;

#include "datum.hh"

sysx::endpoint server_endpoint("127.0.0.1", 10002);
sysx::endpoint client_endpoint("127.0.0.1", 20002);

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
const std::vector<size_t> POWERS = {16,17};
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();

struct Test_web_socket: public Mobile<Test_web_socket> {

	typedef stdx::log<Test_web_socket> this_log;

	Test_web_socket(): _data() {}

	void act() {
		this_log() << "Hello from web socket! Data = " << _data << std::endl;
		_data = 321;
		commit(ext_server());
	}

	void write_impl(packstream& out) {
		out << _data;
	}

	void read_impl(packstream& in) {
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
		commit(ext_server());
	}

	void write_impl(packstream& out) {
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read_impl(packstream& in) {
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

	typedef stdx::log<Sender> this_log;

	explicit Sender(uint32_t n): _input(n) {}

	void act() {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			upstream(ext_server(), new Test_socket(_input));
		}
	}

	void react(Kernel* child) {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

		if (_input.size() != output.size())
			throw Error("input and output size does not match",
				__FILE__, __LINE__, __func__);

		for (size_t i=0; i<_input.size(); ++i) {
			if (_input[i] != output[i]) {
				std::stringstream msg;
				msg << "input and output does not match at i="
					<< i << ",size=" << _input.size() << ":\n"
					<< bits::Bytes<Datum>(_input[i]) << "\n!=\n"
					<< bits::Bytes<Datum>(output[i]);
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}

		this_log() << "Sender::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_KERNELS) {
			commit(the_server());
		}
	}

private:

	uint32_t _num_returned = 0;

	std::vector<Datum> _input;
	uint32_t _sleep = 0;
};

struct Main: public Kernel {

	typedef stdx::log<Main> this_log;

	void act() {
		for (uint32_t i=0; i<POWERS.size(); ++i) {
			size_t sz = 1 << POWERS[i];
			upstream(the_server(), new Sender(sz));
		}
	}

	void react(Kernel*) {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == POWERS.size()) {
			commit(the_server());
		}
	}

private:
	uint32_t _num_returned = 0;
};

uint32_t sleep_time() {
	return sysx::this_process::getenv("SLEEP_TIME", UINT32_C(0));
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 1) {
			sysx::procgroup procs;
			procs.add([&argv] () {
				sysx::this_process::env("START_ID", 1000);
				return sysx::this_process::execute(argv[0], 's');
			});
			// wait for master to start
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			procs.add([&argv] () {
				sysx::this_process::env("START_ID", 2000);
				return sysx::this_process::execute(argv[0], 'c');
			});
			retval = procs.wait();
		} else {
			try {
				if (argc != 2)
					throw std::runtime_error("Wrong number of arguments.");
				the_server()->add_cpu(0);
				if (argv[1][0] == 's') {
					ext_server()->socket(server_endpoint);
					__factory.start();
					__factory.wait();
				}
				if (argv[1][0] == 'c') {
					ext_server()->socket(client_endpoint);
					ext_server()->peer(server_endpoint);
					__factory.start();
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
