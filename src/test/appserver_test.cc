#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/app_server.hh>
#include <factory/server/process_server.hh>

#define XSTRINGIFY(x) STRINGIFY(x)
#define STRINGIFY(x) #x

#if defined(FACTORY_TEST_SERVER)

namespace factory {

	inline namespace daemon_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
//			typedef components::Sub_Iserver<config> remote_server;
			typedef components::Process_iserver<config> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}

}
using namespace factory::daemon_config;
#endif

#if defined(FACTORY_TEST_APP)
namespace factory {

	inline namespace app_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
//			typedef components::Principal_server<config> remote_server;
			typedef components::Process_child_server<config> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}

}
using namespace factory::app_config;
#endif

namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

}

using namespace factory;
using factory::components::Application;

#include "datum.hh"


sysx::endpoint server_endpoint("127.0.0.1", 10000);
sysx::endpoint client_endpoint("127.0.0.1", 20000);

const uint32_t NUM_SIZES = 1;
const uint32_t NUM_KERNELS = 1;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<int> kernel_count(0);

struct Test_socket: public Kernel, Identifiable_tag {

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
		std::cout << "Test_socket::act(): It works!" << std::endl;
		commit(this_server.remote_server());
	}

	void write(sysx::packetstream& out) {
		this_log() << "Test_socket::write()" << std::endl;
		Kernel::write(out);
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read(sysx::packetstream& in) {
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
			[] (sysx::packetstream& in) {
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

const Application::id_type MY_APP_ID = 123;

struct Main: public Kernel, public Identifiable_tag {

	typedef stdx::log<Main> this_log;

	Main(Server& this_server, int argc, char* argv[]) {
		auto& __factory = *this_server.factory();
		__factory.types().register_type(Test_socket::static_type());
		#if defined(FACTORY_TEST_SERVER)
		std::cout << "App = " << XSTRINGIFY(FACTORY_APP_PATH) << std::endl;
		__factory.remote_server()->add(Application(XSTRINGIFY(FACTORY_APP_PATH), MY_APP_ID));
		#endif
		#if defined(FACTORY_TEST_APP)
		Application::id_type app = sysx::this_process::getenv("APP_ID", 0);
		if (app == MY_APP_ID) {
			this_log() << "I am an application no. " << app << "!" << std::endl;
		}
		#endif
	}

	void act(Server& this_server) {
		#if defined(FACTORY_TEST_APP)
		Test_socket* kernel = this_server.factory()->new_kernel<Test_socket>();
		kernel->setapp(sysx::this_process::id());
		upstream(this_server.remote_server(), kernel);
		sleep(3);
		#endif
//		for (uint32_t i=1; i<=NUM_SIZES; ++i)
//			upstream(this_server.local_server(), new Sender(i, _sleep));
	}

	void react(Server& this_server, Kernel*) {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		this_log() << "global kernel count = " << kernel_count << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(this_server.local_server());
		}
	}

private:
	uint32_t _num_returned = 0;
};


int main(int argc, char* argv[]) {
	using namespace factory;
	return factory_main<Main,config>(argc, argv);
}
