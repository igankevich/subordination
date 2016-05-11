#include <factory/algorithm.hh>
#include <factory/cpu_server.hh>
#include <factory/kernel.hh>
#include <factory/server_guard.hh>

#include <sys/cmdline.hh>

#include <cassert>

#include "role.hh"
#include "datum.hh"

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
		// Delete kernel for Valgrind memory checker.
		delete this;
		this_log() << "shutdown counter " << shutdown_counter << std::endl;
		if (++shutdown_counter == TOTAL_NUM_KERNELS/3) {
			this_log() << "go offline" << std::endl;
//			factory()->stop();
			std::exit(0);
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
		this_log()
			<< "parent.id = " << (parent() ? parent()->id() : 12345)
			<< ", principal.id = " << (principal() ? principal()->id() : 12345)
			<< std::endl;
		return _data;
	}

//	static void init_type(Type* t) {
//		t->id(1);
//		t->name("Test_socket");
//	}

private:
	std::vector<Datum> _data;
};

struct Sender: public factory::Kernel {
	typedef stdx::log<Sender> this_log;

	explicit
	Sender(uint32_t n):
	_vector_size(n),
	_input(_vector_size)
	{}

	void act() override {
		this_log() << "Sender "
			<< "id = " << id()
			<< ", parent.id = " << (parent() ? parent()->id() : 12345)
			<< ", principal.id = " << (principal() ? principal()->id() : 12345)
			<< std::endl;
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			factory::upstream(remote_server, this, new Test_socket(_input));
		}
	}

	void react(factory::Kernel* child) override {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		std::vector<Datum> output = test_kernel->data();

		this_log() << "kernel = " << *test_kernel << std::endl;
		#if defined(FACTORY_TEST_OFFLINE)
		assert(not child->from());
		#endif

		if (_input.size() != output.size())
			throw std::runtime_error("test_socket. Input and output size does not match.");

		for (size_t i=0; i<_input.size(); ++i) {
			if (_input[i] != output[i]) {
				std::stringstream msg;
				msg << "test_socket. Input and output does not match: i=" << i << ", ";
				msg << _input[i] << " != " << output[i];
				throw std::runtime_error(msg.str());
			}
		}

		this_log() << "Sender::kernel count = " << _num_returned+1 << std::endl;
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

	typedef stdx::log<Main> this_log;

	void
	act() override {
		for (uint32_t i=0; i<POWERS.size(); ++i) {
			size_t sz = 1 << POWERS[i];
			factory::upstream(local_server, this, new Sender(sz));
		}
	}

	void
	react(factory::Kernel*) override {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == POWERS.size()) {
			factory::commit(local_server, this, factory::Result::success);
		}
	}

private:

	uint32_t _num_returned = 0;

};

int
main(int argc, char* argv[]) {
	Role role = Role::Master;
	sys::cmdline cmdline(argc, argv, {
		sys::cmd::ignore_first_arg(),
		sys::cmd::make_option({"--role"}, role)
	});
	cmdline.parse();
	factory::register_type<Test_socket>();

	if (role == Role::Master) {
		// wait for the child to start
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(remote_server)> g2(remote_server);
	if (role == Role::Slave) {
		remote_server.bind(server_endpoint, netmask);
	}
	if (role == Role::Master) {
		remote_server.bind(client_endpoint, netmask);
		remote_server.peer(server_endpoint);
	}
	local_server.send(new Main);
	int retval = factory::wait_and_return();

	std::clog << "KERNEL count = " << kernel_count << std::endl;
	if (kernel_count > 0) {
		throw factory::Error("some kernels were not deleted",
			__FILE__, __LINE__, __func__);
	}
	if (kernel_count < 0) {
		throw factory::Error("some kernels were deleted multiple times",
			__FILE__, __LINE__, __func__);
	}
	return retval;
}
