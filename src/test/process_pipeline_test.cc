#include <factory/api.hh>
#include <factory/base/error_handler.hh>
#include <factory/registry.hh>

#define XSTRINGIFY(x) STRINGIFY(x)
#define STRINGIFY(x) #x

using namespace factory::api;
using factory::Application;

#include "datum.hh"


const uint32_t NUM_SIZES = 10;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public Kernel {

	Test_socket(): _data() {
	}

	explicit Test_socket(const std::vector<Datum>& x): _data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void act() override {
		sys::log_message("chld", "Test_socket::act(): It works!");
		commit<Remote>(this);
	}

	void write(sys::pstream& out) override {
		sys::log_message("chld", "Test_socket::write()");
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read(sys::pstream& in) override {
		sys::log_message("chld", "Test_socket::read()");
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

	void act(Pipeline& this_pipeline) {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
			upstream(this_pipeline.remote_pipeline(), new Test_socket(_input));
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
			commit(this_pipeline.local_pipeline());
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
		for (uint32_t i=1; i<=NUM_SIZES; ++i) {
			sys::log_message("chld", "sent _/_", i, NUM_SIZES);
			Test_socket* kernel = new Test_socket;
			kernel->setapp(sys::this_process::id());
			upstream<Remote>(this, kernel);
		}
	}

	void react(Kernel*) override {
		sys::log_message("chld", "returned _/_", _num_returned+1, NUM_SIZES);
		if (++_num_returned == NUM_SIZES) {
			sys::log_message("chld", "finished");
			commit<Local>(this, factory::Result::success);
		}
	}

private:
	uint32_t _num_returned = 0;
};


int main(int argc, char* argv[]) {
	using namespace factory::api;
	factory::install_error_handler();
	factory::types.register_type<Test_socket>();
	Factory_guard g;
	#if defined(FACTORY_TEST_SERVER)
	Application app(XSTRINGIFY(FACTORY_APP_PATH));
	factory::factory.child().add(app);
	std::this_thread::sleep_for(std::chrono::seconds(5));
	factory::graceful_shutdown(0);
	#else
	send<Local>(new Main);
	#endif
	return factory::wait_and_return();
}
