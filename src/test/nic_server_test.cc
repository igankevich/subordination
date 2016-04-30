#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>

#include <cassert>

#include "datum.hh"

#if defined(FACTORY_TEST_WEBSOCKET)
#include <factory/server/nic_server.hh>
#include <web/websocket.hh>
#define FACTORY_SOCKET_TYPE factory::components::Web_socket
#endif

#if defined(FACTORY_TEST_APPSERVER)
#include <factory/server/app_server.hh>
#endif

#if !defined(FACTORY_SOCKET_TYPE)
#include <factory/server/nic_server.hh>
#define FACTORY_TEST_SOCKET
#define FACTORY_SOCKET_TYPE sys::socket
#endif

namespace factory {
#if defined(FACTORY_TEST_SOCKET) || defined(FACTORY_TEST_WEBSOCKET)
	inline namespace this_config {

		struct config {
			typedef components::Server<config> server;
			typedef components::Principal kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, FACTORY_SOCKET_TYPE> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> io_server;
			typedef components::No_server<config> app_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
#endif

#if defined(FACTORY_TEST_APPSERVER)
	inline namespace this_config {

		struct config {
			typedef components::Server<config> server;
			typedef components::Principal kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::Sub_Iserver<config> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> io_server;
			typedef components::No_server<config> app_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
#endif
}

using namespace factory;
using namespace factory::this_config;

namespace stdx {

	template<>
	struct disable_log_category<sys::buffer_category>:
	public std::integral_constant<bool, true> {};

}

const sys::ipv4_addr netmask = sys::ipaddr_traits<sys::ipv4_addr>::loopback_mask();
#if defined(FACTORY_TEST_OFFLINE)
sys::endpoint server_endpoint("127.0.0.1", 10001);
sys::endpoint client_endpoint({127,0,0,1}, 20001);
#else
sys::endpoint server_endpoint("127.0.0.1", 10002);
sys::endpoint client_endpoint({127,0,0,1}, 20002);
#endif

//const std::vector<size_t> POWERS = {1,2,3,4,16,17};
const std::vector<size_t> POWERS = {1,2,3,4};
//const std::vector<size_t> POWERS = {16,17};
const uint32_t NUM_KERNELS = 7;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * POWERS.size();
//const uint32_t NUM_SIZES = 1;
//const uint32_t NUM_KERNELS = 2;
//const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);
std::atomic<uint32_t> shutdown_counter(0);

struct Test_socket: public Kernel {

	typedef stdx::log<Test_socket> this_log;

	Test_socket(): _data() {
		++kernel_count;
	}

	explicit Test_socket(std::vector<Datum> x): _data(x) {
		++kernel_count;
	}

	virtual ~Test_socket() {
		--kernel_count;
	}

	void act() override {
		this_log() << "act: kernel no. "
			<< kernel_count << std::endl;
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
		commit(remote_server());
		#endif
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void
	read(sys::packetstream& in) override {
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
			[] (sys::packetstream& in) {
				Test_socket* k = new Test_socket;
				k->read(in);
				return k;
			}
		};
	}

//	static void init_type(Type* t) {
//		t->id(1);
//		t->name("Test_socket");
//	}

private:
	std::vector<Datum> _data;
};

struct Sender: public Kernel {
	typedef stdx::log<Sender> this_log;

	Sender(uint32_t n, uint32_t s):
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
			upstream(remote_server(), new Test_socket(_input));
		}
	}

	void react(Kernel* child) override {

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
			commit(local_server());
		}
	}

private:

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
};

struct Main: public Kernel {

	typedef stdx::log<Main> this_log;

	Main(Server& this_server, int argc, char* argv[]) {
		if (argc != 3)
			throw std::runtime_error("Wrong number of arguments.");
		_role = argv[1][0];
		if (_role == 'x') {
			this_server.factory()->remote_server()->bind(server_endpoint, netmask);
		}
		if (_role == 'y') {
			this_server.factory()->remote_server()->bind(client_endpoint, netmask);
			this_server.factory()->remote_server()->peer(server_endpoint);
		}
	}

	void
	act() override {
		factory()->types().register_type(Test_socket::static_type());
//		factory()->dump_hierarchy(std::cout);
		if (_role == 'y') {
			for (uint32_t i=0; i<POWERS.size(); ++i) {
				size_t sz = 1 << POWERS[i];
				upstream(local_server(), new Sender(sz, _sleep));
			}
		}
	}

	void
	react(Kernel*) override {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == POWERS.size()) {
			commit(local_server());
		}
	}

private:
	uint32_t _num_returned = 0;
	uint32_t _sleep = 0;
	char _role = 'm';
};

int
main(int argc, char* argv[]) {
	using namespace factory;
	struct mainfunc {};
	typedef stdx::log<mainfunc> this_log;
	int retval = 0;
	if (argc <= 1) {
		this_log()
			<< "server=" << server_endpoint
			<< ",client=" << client_endpoint
			<< std::endl;
		uint32_t sleep = 0;
		sys::process_group procs;
		procs.add([&argv, sleep] () {
			return sys::this_process::execute(argv[0], 'x', sleep);
		});
		// wait for the child to start
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		procs.add([&argv, sleep] () {
			return sys::this_process::execute(argv[0], 'y', sleep);
		});
		this_log() << "sys::process group = " << procs << std::endl;
		const sys::proc_status stat = procs.back().wait();
		this_log() << "master process terminated: " << stat << std::endl;
		procs.front().terminate();
		const sys::proc_status stat2 = procs.front().wait();
		this_log() << "child process terminated: " << stat2 << std::endl;
		retval |= stat.exit_code() | sys::signal_type(stat.term_signal());
		retval |= stat2.exit_code() | sys::signal_type(stat2.term_signal());
	} else {
		retval = factory_main<Main,config>(argc, argv);
	}
	std::cout << "KERNEL count = " << kernel_count << std::endl;
	if (kernel_count > 0) {
		throw components::Error("some kernels were not deleted",
			__FILE__, __LINE__, __func__);
	}
	if (kernel_count < 0) {
		throw components::Error("some kernels were deleted multiple times",
			__FILE__, __LINE__, __func__);
	}
	return retval;
}
