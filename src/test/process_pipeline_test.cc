#include <factory/api.hh>
#include <factory/base/error_handler.hh>

#define XSTRINGIFY(x) STRINGIFY(x)
#define STRINGIFY(x) #x

using namespace asc;
using asc::application;

#include "datum.hh"


const uint32_t NUM_SIZES = 10;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public kernel {

	Test_socket():
	_data() {}

	explicit Test_socket(const std::vector<Datum>& x):
	_data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void
	act() override {
		sys::log_message("tst", "Test_socket::act(): It works!");
		commit<Remote>(this);
	}

	void
	write(sys::pstream& out) override {
		sys::log_message("tst", "Test_socket::write()");
		kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void
	read(sys::pstream& in) override {
		sys::log_message("tst", "Test_socket::read()");
		kernel::read(in);
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum>
	data() const {
		return _data;
	}

private:
	std::vector<Datum> _data;
};

struct Main: public kernel {

	void
	act() override {
		for (uint32_t i=1; i<=NUM_SIZES; ++i) {
			sys::log_message("tst", "sent _/_", i, NUM_SIZES);
			Test_socket* k = new Test_socket;
//			k->setapp(sys::this_process::id());
			upstream<Remote>(this, k);
		}
	}

	void
	react(kernel*) override {
		sys::log_message("tst", "returned _/_", _num_returned+1, NUM_SIZES);
		if (++_num_returned == NUM_SIZES) {
			sys::log_message("tst", "finished");
			commit<Local>(this, asc::exit_code::success);
		}
	}

private:
	uint32_t _num_returned = 0;
};


int
main(int argc, char* argv[]) {
	using namespace asc;
	install_error_handler();
	types.register_type<Test_socket>();
	factory_guard g;
	#if defined(FACTORY_TEST_SERVER)
	application app({XSTRINGIFY(FACTORY_APP_PATH)}, {});
	factory.child().add(app);
	std::this_thread::sleep_for(std::chrono::seconds(5));
	graceful_shutdown(0);
	#else
	sys::fd_type in = this_application::get_input_fd();
	sys::fd_type out = this_application::get_output_fd();
	sys::log_message("tst", "in = _, out = _", in, out);
	send(new Main);
	#endif
	return wait_and_return();
}
