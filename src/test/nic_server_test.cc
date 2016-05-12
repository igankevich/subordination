#include <factory/algorithm.hh>
#include <factory/cpu_server.hh>
#include <factory/kernel.hh>
#include <factory/server_guard.hh>

#include <sys/cmdline.hh>
#include <sys/log.hh>

#include <cassert>

#include "role.hh"
#include "datum.hh"
#include "test.hh"

#if defined(FACTORY_TEST_WEBSOCKET)
#include <factory/nic_server.hh>
#include <web/websocket.hh>
#define FACTORY_SOCKET_TYPE factory::Web_socket
#endif

#if !defined(FACTORY_SOCKET_TYPE)
#include <factory/nic_server.hh>
#define FACTORY_TEST_SOCKET
#define FACTORY_SOCKET_TYPE sys::socket
#endif

using test::Role;

factory::CPU_server<factory::Kernel> local_server;

#if defined(FACTORY_TEST_SOCKET) || defined(FACTORY_TEST_WEBSOCKET)
struct Router {

	void
	send_local(factory::Kernel* rhs) {
		local_server.send(rhs);
	}

	void
	send_remote(factory::Kernel*);

	void
	forward(const factory::Kernel_header& hdr, sys::packetstream& istr) {
		assert(false);
	}

};

factory::NIC_server<factory::Kernel, FACTORY_SOCKET_TYPE, Router> remote_server;

void
Router::send_remote(factory::Kernel* rhs) {
	remote_server.send(rhs);
}
#endif

namespace stdx {

	template<>
	struct enable_log<factory::server_category>: public std::true_type {};

	template<>
	struct enable_log<factory::kernel_category>: public std::true_type {};

}


const sys::ipv4_addr netmask = sys::ipaddr_traits<sys::ipv4_addr>::loopback_mask();
#if defined(FACTORY_TEST_OFFLINE)
sys::endpoint server_endpoint({127,0,0,1}, 10001);
sys::endpoint client_endpoint({127,0,0,1}, 20001);
#else
sys::endpoint server_endpoint({127,0,0,1}, 10002);
sys::endpoint client_endpoint({127,0,0,1}, 20002);
#endif

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
const std::vector<size_t> POWERS = {1,2,3,4};
//const std::vector<size_t> POWERS = {16,17};
const uint32_t NUM_KERNELS = 7;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();

std::atomic<int> kernel_count(0);
std::atomic<uint32_t> shutdown_counter(0);

Role role = Role::Master;

struct Test_socket: public factory::Kernel {

	typedef stdx::log<Test_socket> this_log;

	Test_socket():
	_data()
	{ ++kernel_count; }

	explicit
	Test_socket(std::vector<Datum> x):
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
			factory::commit(local_server, this);
		}
		#else
		factory::commit(remote_server, this);
		#endif
	}

	void
	write(sys::packetstream& out) override {
		factory::Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void
	read(sys::packetstream& in) override {
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
			factory::upstream(remote_server, this, new Test_socket(_input));
		}
	}

	void react(factory::Kernel* child) override {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

//		#if defined(FACTORY_TEST_OFFLINE)
//		assert(not child->from());
//		#endif

		test::equal(_input.size(), output.size(), "Input and output size does not match");
		test::compare(_input, output,  "Input and output data does not match");

		if (++_num_returned == NUM_KERNELS) {
			factory::commit(local_server, this);
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
			factory::upstream(local_server, this, new Sender(sz));
		}
	}

	void
	react(factory::Kernel*) override {
		if (++_num_returned == POWERS.size()) {
			factory::commit(local_server, this, factory::Result::success);
		}
	}

private:

	uint32_t _num_returned = 0;

};

int
main(int argc, char* argv[]) {
	sys::syslog_guard g0(std::clog, sys::syslog_guard::tee);
	sys::this_process::ignore_signal(sys::signal::broken_pipe);
	sys::cmdline cmdline(argc, argv, {
		sys::cmd::ignore_first_arg(),
		sys::cmd::make_option({"--role"}, role)
	});
	cmdline.parse();
	factory::register_type<Test_socket>();

	if (role == Role::Slave) {
		remote_server.bind(server_endpoint, netmask);
	}
	if (role == Role::Master) {
		remote_server.bind(client_endpoint, netmask);
		// wait for the child to start
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		remote_server.peer(server_endpoint);
	}

	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(remote_server)> g2(remote_server);
	if (role == Role::Master) {
		local_server.send(new Main);
	}

	int retval = factory::wait_and_return();

	#if defined(FACTORY_TEST_OFFLINE)
	if (role == Role::Master)
	#endif
	{
		std::clog << "KERNEL count = " << kernel_count << std::endl;
		if (kernel_count > 0) {
			throw factory::Error("some kernels were not deleted",
				__FILE__, __LINE__, __func__);
		}
		if (kernel_count < 0) {
			throw factory::Error("some kernels were deleted multiple times",
				__FILE__, __LINE__, __func__);
		}
	}
	return retval;
}
