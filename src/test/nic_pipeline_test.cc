#include <factory/base/error_handler.hh>
#include <factory/api.hh>

#include <unistdx/base/cmdline>

#include "role.hh"
#include "datum.hh"

#include <gtest/gtest.h>

using test::Role;

const sys::ipv4_addr netmask = sys::ipaddr_traits<sys::ipv4_addr>::loopback_mask();
#if defined(FACTORY_TEST_OFFLINE)
sys::endpoint principal_endpoint({127,0,0,1}, 10001);
sys::endpoint subordinate_endpoint({127,0,0,1}, 20001);
#else
sys::endpoint principal_endpoint({127,0,0,1}, 10002);
sys::endpoint subordinate_endpoint({127,0,0,1}, 20002);
#endif

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
const std::vector<size_t> POWERS = {1,2,3,4};
//const std::vector<size_t> POWERS = {16,17};
const uint32_t NUM_KERNELS = 7;
#if defined(FACTORY_TEST_OFFLINE)
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();
#endif

std::atomic<int> kernel_count(0);
std::atomic<uint32_t> shutdown_counter(0);

Role role = Role::Master;

using namespace factory::api;

struct Test_socket: public factory::Kernel {

	Test_socket():
	_data()
	{ ++kernel_count; }

	explicit
	Test_socket(const std::vector<Datum>& x):
	_data(x)
	{ ++kernel_count; }

	virtual
	~Test_socket() {
		--kernel_count;
	}

	void act() override {
		//std::clog << "Test_socket::act()" << std::endl;
		#if defined(FACTORY_TEST_OFFLINE)
		if (role == Role::Slave) {
			// Delete kernel for Valgrind memory checker.
			delete this;
			if (++shutdown_counter == TOTAL_NUM_KERNELS/3) {
				std::clog << "go offline" << std::endl;
				factory::graceful_shutdown(0);
			}
		} else {
			commit<Local>(this);
		}
		#else
		commit<Remote>(this);
		#endif
	}

	void
	write(sys::pstream& out) override {
		factory::Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void
	read(sys::pstream& in) override {
		factory::Kernel::read(in);
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

struct Sender: public factory::Kernel {

	explicit
	Sender(uint32_t n):
	_vector_size(n),
	_input(_vector_size)
	{}

	void act() override {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			upstream<Remote>(this, new Test_socket(_input));
		}
	}

	void react(factory::Kernel* child) override {
		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();
		EXPECT_EQ(this->_input.size(), output.size());
		EXPECT_EQ(this->_input, output);
		if (++_num_returned == NUM_KERNELS) {
			commit<Local>(this);
		}
	}

private:

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
};

struct Main: public factory::Kernel {

	void
	act() override {
		for (uint32_t i=0; i<POWERS.size(); ++i) {
			size_t sz = 1 << POWERS[i];
			upstream<Local>(this, new Sender(sz));
		}
	}

	void
	react(factory::Kernel*) override {
		if (++_num_returned == POWERS.size()) {
			commit<Local>(this, factory::Result::success);
		}
	}

private:

	uint32_t _num_returned = 0;

};

TEST(NICServerTest, All) {
	using factory::factory;
	factory::register_type<Test_socket>();
	if (role == Role::Slave) {
		factory.nic().bind(principal_endpoint, netmask);
	}
	if (role == Role::Master) {
		factory.nic().bind(subordinate_endpoint, netmask);
		// wait for the child to start
		using namespace std::this_thread;
		using namespace std::chrono;
		sleep_for(milliseconds(1000));
		factory.nic().peer(principal_endpoint);
	}

	Factory_guard g;
	if (role == Role::Master) {
		send<Local>(new Main);
	}

	int retval = factory::wait_and_return();

	#if defined(FACTORY_TEST_OFFLINE)
	if (role == Role::Master)
	#endif
	{
		EXPECT_EQ(0, kernel_count) << "some kernels were not deleted"
			" or were deleted multiple times";
	}
	EXPECT_EQ(0, retval);
}

int
main(int argc, char* argv[]) {
	factory::install_error_handler();
	// init gtest without arguments to pass custom arguments
	// from custom test runner
	int no_argc = 0;
	char** no_argv = nullptr;
	::testing::InitGoogleTest(&no_argc, no_argv);
	sys::this_process::ignore_signal(sys::signal::broken_pipe);
	sys::input_operator_type options[] = {
		sys::ignore_first_argument(),
		sys::make_key_value("role", role),
		nullptr
	};
	sys::parse_arguments(argc, argv, options);
	return RUN_ALL_TESTS();
}
