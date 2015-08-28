#include <factory/factory.hh>
#include <factory/cfg/standard.hh>

using namespace factory;
using namespace factory::standard_config;
using factory::components::Application;

#include "datum.hh"

sysx::endpoint server_endpoint("127.0.0.1", 10000);
sysx::endpoint client_endpoint("127.0.0.1", 20000);

const uint32_t NUM_SIZES = 1;
const uint32_t NUM_KERNELS = 2;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public Kernel {

	typedef stdx::log<Test_socket> this_log;

	Test_socket(): _data() {
	}

	explicit Test_socket(std::vector<Datum> x): _data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void act(Server& this_server) {
		this_log() << "Test_socket::act(): It works!" << std::endl;
//		commit(remote_server());
	}

	void write(sysx::packstream& out) {
		this_log() << "Test_socket::write()" << std::endl;
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read(sysx::packstream& in) {
		this_log() << "Test_socket::read()" << std::endl;
		Kernel::read(in);
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

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			1,
			"Test_socket",
			[] (sysx::packstream& in) {
				Test_socket* k = new Test_socket;
				k->read(in);
				return k;
			}
		};
	}
	
private:
	std::vector<Datum> _data;
};

/*
struct Sender: public Kernel, public Identifiable_tag {

	typedef stdx::log<Sender> this_log;

	Sender(uint32_t n, uint32_t s):
		_vector_size(n),
		_input(_vector_size),
		_sleep(s) {}

	void act(Server& this_server) {
		this_log() << "Sender "
			<< "id = " << id()
			<< ", parent.id = " << (parent() ? parent()->id() : 12345)
			<< ", principal.id = " << (principal() ? principal()->id() : 12345)
			<< std::endl;
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
			upstream(this_server.remote_server(), new Test_socket(_input));
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
*/

const Application::id_type MY_APP_ID = 123;

struct Main: public Kernel {

	typedef stdx::log<Main> this_log;

	Main(Server& this_server, int argc, char* argv[]) {
		auto& __factory = *this_server.factory();
		Application::id_type app = sysx::this_process::getenv("APP_ID", 0);
		if (app == MY_APP_ID) {
			this_log() << "I am an application no. " << app << "!" << std::endl;
			__factory.setrole(Factory::Role::Subordinate);
			__factory.start();
			sleep(3);
			__factory.stop();
			__factory.wait();
		} else {
			__factory.app_server()->add(Application(argv[0], MY_APP_ID));
			__factory.setrole(Factory::Role::Principal);
			__factory.start();
			the_server()->send(new Main);
			sleep(3);
			__factory.stop();
			__factory.wait();
		}
	}

	void act(Server& this_server) {
		Test_socket* kernel = new Test_socket;
		kernel->from(server_endpoint);
		kernel->setapp(MY_APP_ID);
		kernel->parent(this);
		app_server()->send(kernel);
//		for (uint32_t i=1; i<=NUM_SIZES; ++i)
//			upstream(the_server(), new Sender(i, _sleep));
	}

	void react(Server& this_server, Kernel*) {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(the_server());
		}
	}

private:
	uint32_t _num_returned = 0;
};


int main(int argc, char* argv[]) {
	using namespace factory;
	return factory_main<Main,config>(argc, argv);
}
