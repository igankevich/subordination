#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/process_server.hh>
#include <factory/server_guard.hh>
#include <factory/reflection.hh>
#include <factory/factory.hh>

#define XSTRINGIFY(x) STRINGIFY(x)
#define STRINGIFY(x) #x

namespace factory {
#if defined(FACTORY_TEST_SERVER)
	struct Router {

		void
		send_local(Kernel* rhs) {
			local_server.send(rhs);
		}

		void
		send_remote(Kernel*) {
			assert(false);
		}

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr);

	};

	Process_iserver<Kernel,Router> remote_server;

	void
	Router::forward(const Kernel_header& hdr, sys::packetstream& istr) {
		remote_server.forward(hdr, istr);
	}
#else
	struct Router {

		void
		send_local(Kernel* rhs) {
			local_server.send(rhs);
		}

		void
		send_remote(Kernel*);

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
			assert(false);
		}

	};

	Process_child_server<Kernel,Router> remote_server;

	void
	Router::send_remote(Kernel* rhs) {
		remote_server.send(rhs);
	}
#endif
}

using namespace factory;
using factory::Application;

#include "datum.hh"


sys::endpoint server_endpoint("127.0.0.1", 10000);
sys::endpoint client_endpoint("127.0.0.1", 20000);

const uint32_t NUM_SIZES = 1;
const uint32_t NUM_KERNELS = 1;
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

	void act() override {
		std::cout << "Test_socket::act(): It works!" << std::endl;
		commit(remote_server, this);
	}

	void write(sys::packetstream& out) override {
		this_log() << "Test_socket::write()" << std::endl;
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read(sys::packetstream& in) override {
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

private:
	std::vector<Datum> _data;
};

/*
struct Sender: public Kernel {

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
			commit(this_server.local_server());
		}
	}

private:

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
	uint32_t _sleep = 0;
};
*/

struct Main: public Kernel {

	typedef stdx::log<Main> this_log;

	Main() {
		factory::types.register_type<Test_socket>();
		#if defined(FACTORY_TEST_SERVER)
		Application app(XSTRINGIFY(FACTORY_APP_PATH));
		std::cout << "App = " << app << std::endl;
		remote_server.add(app);
		#endif
	}

	void act() override {
		#if defined(FACTORY_TEST_APP)
		Test_socket* kernel = new Test_socket;
		kernel->setapp(sys::this_process::id());
		upstream(remote_server, this, kernel);
		#endif
		#if defined(FACTORY_TEST_SERVER)
		std::this_thread::sleep_for(std::chrono::seconds(5));
		commit(local_server, this);
		#endif
//		for (uint32_t i=1; i<=NUM_SIZES; ++i)
//			upstream(local_server(), new Sender(i, _sleep));
	}

	void react(Kernel*) override {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(this);
		}
	}

private:
	uint32_t _num_returned = 0;
};


int main(int argc, char* argv[]) {
	local_server.send(new Main);
	return factory::wait_and_return();
}
