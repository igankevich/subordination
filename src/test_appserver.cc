//#include <factory/factory.hh>
#include <libfactory.cc>

using namespace factory;

#include "datum.hh"

sysx::endpoint server_endpoint("127.0.0.1", 10000);
sysx::endpoint client_endpoint("127.0.0.1", 20000);

const uint32_t NUM_SIZES = 1;
const uint32_t NUM_KERNELS = 2;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public Mobile<Test_socket> {

	typedef stdx::log<Test_socket> this_log;

	Test_socket(): _data() {
	}

	explicit Test_socket(std::vector<Datum> x): _data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void act() {
		this_log() << "Test_socket::act(): It works!" << std::endl;
//		commit(remote_server());
	}

	void write_impl(packstream& out) {
		this_log() << "Test_socket::write_impl()" << std::endl;
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read_impl(packstream& in) {
		this_log() << "Test_socket::read_impl()" << std::endl;
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum> data() const { 
		this_log()
			<< "parent.id = " << (parent() ? parent()->id() : 12345)
			<< ", principal.id = " << (principal() ? principal()->id() : 12345)
			<< std::endl;
		return _data;
	}

	static void init_type(Type* t) {
		t->id(1);
		t->name("Test_socket");
	}
	
private:
	std::vector<Datum> _data;
};

struct Sender: public Identifiable<Kernel> {

	typedef stdx::log<Sender> this_log;

	Sender(uint32_t n, uint32_t s):
		_vector_size(n),
		_input(_vector_size),
		_sleep(s) {}

	void act() {
		this_log() << "Sender "
			<< "id = " << id()
			<< ", parent.id = " << (parent() ? parent()->id() : 12345)
			<< ", principal.id = " << (principal() ? principal()->id() : 12345)
			<< std::endl;
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
			upstream(remote_server(), new Test_socket(_input));
//			this_log() << " Sender id = " << this->id() << std::endl;
		}
	}

	void react(Kernel* child) {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

		this_log() << "kernel = " << *test_kernel << std::endl;

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

		this_log() << "Sender::kernel count = " << _num_returned+1 << std::endl;
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

const Application::id_type MY_APP_ID = 123;
struct Main: public Kernel {

	typedef stdx::log<Main> this_log;

	Main(uint32_t s=0): _sleep(s) {}

	void act() {
		Test_socket* kernel = new Test_socket;
		kernel->from(server_endpoint);
		kernel->setapp(MY_APP_ID);
		kernel->parent(this);
		app_server()->send(kernel);
//		for (uint32_t i=1; i<=NUM_SIZES; ++i)
//			upstream(the_server(), new Sender(i, _sleep));
	}

	void react(Kernel*) {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(the_server());
		}
	}

private:
	uint32_t _num_returned = 0;
	uint32_t _sleep = 0;
};

uint32_t sleep_time() {
	return sysx::this_process::getenv("SLEEP_TIME", UINT32_C(0));
}



struct App {
	typedef stdx::log<App> this_log;
	int run(int argc, char* argv[]) {
		Application::id_type app = sysx::this_process::getenv("APP_ID", 0);
		if (app == MY_APP_ID) {
			this_log() << "I am an application no. " << app << "!" << std::endl;
			the_server()->add_cpu(0);
			__factory.setrole(Factory::Role::Subordinate);
			__factory.start();
			sleep(3);
			__factory.stop();
			__factory.wait();
		} else {
			the_server()->add_cpu(0);
			app_server()->add(Application(argv[0], MY_APP_ID));
			__factory.setrole(Factory::Role::Principal);
			__factory.start();
			the_server()->send(new Main);
			sleep(3);
			__factory.stop();
			__factory.wait();
		}
		return 0;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
