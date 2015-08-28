#include <factory/factory.hh>
#include <factory/cfg/local.hh>

using namespace factory;
using namespace local_config;
using factory::components::Error;

std::atomic<int32_t> kernel_count(0);

struct Sender: public Kernel {

	Sender() { ++kernel_count; }
	~Sender() { --kernel_count; }

	void act(Server& this_server) override {
//		throw Error("act() is called, but it should not",
//			__FILE__, __LINE__, __func__);
	}
};

struct Main: public Kernel {

	Main(Server& this_server, int argc, char* argv[])
	{ ++kernel_count; }

	~Main()
	{ --kernel_count; }

	void act(Server& this_server) override {
		for (uint32_t i=0; i<10; ++i)
			upstream(this_server.local_server(), new Sender);
		this_server.factory()->stop();
	}

	void react(Server&, Kernel*) override {
		throw Error("react() is called, but it should not",
			__FILE__, __LINE__, __func__);
	}

};

int
main(int argc, char* argv[]) {
	using namespace factory;
	int ret = factory_main<Main,config>(argc, argv);
	if (kernel_count > 0) {
		throw Error("some kernels were not deleted",
			__FILE__, __LINE__, __func__);
	}
	if (kernel_count < 0) {
		throw Error("some kernels were deleted multiple times",
			__FILE__, __LINE__, __func__);
	}
	return ret;
}
