#include <stdx/debug.hh>

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


const uint32_t NUM_SIZES = 1;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public Kernel {

	Test_socket(): _data() {
	}

	explicit Test_socket(std::vector<Datum> x): _data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void act() override {
		stdx::debug_message("chld", "Test_socket::act(): It works!");
		factory::commit(factory::remote_server, this);
	}

	void write(sys::packetstream& out) override {
		stdx::debug_message("chld", "Test_socket::write()");
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read(sys::packetstream& in) override {
		stdx::debug_message("chld", "Test_socket::read()");
		Kernel::read(in);
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum> data() const {
		return _data;
	}

private:
	std::vector<Datum> _data;
};

/*
struct Sender: public Kernel {

	Sender(uint32_t n, uint32_t s):
		_vector_size(n),
		_input(_vector_size),
		_sleep(s) {}

	void act(Server& this_server) {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
			upstream(this_server.remote_server(), new Test_socket(_input));
		}
	}

	void react(Kernel* child) {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
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

	void act() override {
		Test_socket* kernel = new Test_socket;
		kernel->setapp(sys::this_process::id());
		upstream(remote_server, this, kernel);
//		for (uint32_t i=1; i<=NUM_SIZES; ++i)
//			upstream(local_server(), new Sender(i, _sleep));
	}

	void react(Kernel*) override {
		if (++_num_returned == NUM_SIZES) {
			#ifndef NDEBUG
			stdx::debug_message("chld", "finished");
			#endif
			factory::commit(local_server, this, factory::Result::success);
		}
	}

private:
	uint32_t _num_returned = 0;
};


int main(int argc, char* argv[]) {
	factory::Terminate_guard g0;
	factory::types.register_type<Test_socket>();
	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(remote_server)> g2(remote_server);
	#if defined(FACTORY_TEST_SERVER)
	Application app(XSTRINGIFY(FACTORY_APP_PATH));
	remote_server.add(app);
	std::this_thread::sleep_for(std::chrono::seconds(5));
	factory::graceful_shutdown(0);
	#else
	local_server.send(new Main);
	#endif
	return factory::wait_and_return();
}
