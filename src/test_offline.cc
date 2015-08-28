#include <factory/factory.hh>
#include <factory/cfg/minimal.hh>
#include "datum.hh"

using namespace factory;
using namespace minimal_config;
using factory::components::Result;

sysx::endpoint server_endpoint("127.0.0.1", 10001);
sysx::endpoint client_endpoint("127.0.0.1", 20001);

const uint32_t NUM_SIZES = 13;
const uint32_t NUM_KERNELS = 7;
const uint32_t TOTAL_NUM_KERNELS = NUM_KERNELS * NUM_SIZES;

std::atomic<uint32_t> shutdown_counter(0);

struct Test_socket: public Kernel {

	typedef stdx::log<Test_socket> this_log;

	Test_socket(): _data() {}
	explicit Test_socket(std::vector<Datum> x): _data(x) {}

	void act(Server& this_server) override {
		// Delete kernel for Valgrind memory checker.
		delete this;
		if (++shutdown_counter == TOTAL_NUM_KERNELS) {
			this_server.factory()->stop();
		}
	}

	void write_impl(sysx::packstream& out) {
		out << uint32_t(_data.size());
		for (size_t i=0; i<_data.size(); ++i)
			out << _data[i];
	}

	void read_impl(sysx::packstream& in) {
		uint32_t sz;
		in >> sz;
		_data.resize(sz);
		for (size_t i=0; i<_data.size(); ++i)
			in >> _data[i];
	}

	std::vector<Datum> data() const { return _data; }

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

struct Sender: public Kernel, public Identifiable_tag {

	typedef stdx::log<Sender> this_log;

	explicit Sender(uint32_t n):
		_vector_size(n),
		_input(_vector_size) {}

	void act(Server& this_server) override {
		for (uint32_t i=0; i<NUM_KERNELS; ++i) {
			upstream(this_server.remote_server(), new Test_socket(_input));
			++shutdown_counter;
			this_log() << " Sender id = " << this->id() << std::endl;
			this_log() << " kernel count2 = " << shutdown_counter << std::endl;
		}
	}

	void react(Server& this_server, Kernel* child) override {

		Test_socket* test_kernel = dynamic_cast<Test_socket*>(child);
		if (test_kernel->result() != Result::ENDPOINT_NOT_CONNECTED) {
			std::stringstream msg;
			msg << "Bad test kernel result: result=" << test_kernel->result();
			throw std::runtime_error(msg.str());
		}

		this_log() << "Sender::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_KERNELS) {
			commit(this_server.local_server());
		}
	}

	uint32_t _num_returned = 0;
	uint32_t _vector_size;

	std::vector<Datum> _input;
};

struct Main: public Kernel {

	typedef stdx::log<Main> this_log;
	using typename Kernel::server_type;

	Main(Server& this_server, int argc, char* argv[]) {
		auto& __factory = *this_server.factory();
		if (argc <= 1) {
			this_log()
				<< "server=" << server_endpoint
				<< ",client=" << client_endpoint
				<< std::endl;
			uint32_t sleep = 0;
			sysx::procgroup procs;
			procs.add([&argv, sleep] () {
				sysx::this_process::env("START_ID", 1000);
				return sysx::this_process::execute(argv[0], 'x', sleep);
			});
			// wait for master to start
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			procs.add([&argv, sleep] () {
				sysx::this_process::env("START_ID", 2000);
				return sysx::this_process::execute(argv[0], 'y', sleep);
			});
			this_log() << "sysx::proc group = " << procs << std::endl;
			procs.wait([] (const sysx::proc& proc, sysx::proc_info stat) {
				this_log() << "proc exited proc=" << proc
					<< ",status=" << stat.exit_code() << std::endl;
			});
			this_log() << "sysx::log test " << std::endl;
			_role = 'm';
//			retval = procs.wait();
		} else {
			__factory.types().register_type(Test_socket::static_type());
			__factory.dump_hierarchy(std::cout);
			if (argc != 3)
				throw std::runtime_error("Wrong number of arguments.");
			if (argv[1][0] == 'x') {
				__factory.remote_server()->socket(server_endpoint);
				_role = 'x';
			}
			if (argv[1][0] == 'y') {
				__factory.remote_server()->socket(client_endpoint);
				__factory.remote_server()->peer(server_endpoint);
				_role = 'y';
			}
		}
	}

	void act(Server& this_server) override {
		if (_role == 'y') {
			for (uint32_t i=1; i<=NUM_SIZES; ++i)
				upstream(this_server.local_server(), this_server.factory()->new_kernel<Sender>(i));
		}
		if (_role == 'm') {
			commit(this_server.local_server());
		}
	}

	void react(Server& this_server, Kernel*) override {
		this_log() << "Main::kernel count = " << _num_returned+1 << std::endl;
		if (++_num_returned == NUM_SIZES) {
			commit(this_server.local_server());
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
	return factory_main<Main,config>(argc, argv);
}
