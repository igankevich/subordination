#include <factory/factory.hh>
using namespace factory;

std::atomic<uint32_t> kernel_count(0);

struct Sender: public Kernel {

	Sender() { kernel_count++; }
	virtual ~Sender() { kernel_count--; }

	void act() {
//		throw Error("act() is called, but it should not",
//			__FILE__, __LINE__, __func__);
	}
};

struct Main: public Kernel {

	Main() { kernel_count++; }
	virtual ~Main() { kernel_count--; }

	void act() {
		for (uint32_t i=0; i<10; ++i)
			upstream(the_server(), new Sender);
		__factory.stop_now();
	}

	void react(Kernel*) {
		throw Error("react() is called, but it should not",
			__FILE__, __LINE__, __func__);
	}

};

struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		try {
			the_server()->add_cpu(0);
			the_server()->add_cpu(1);
			the_server()->send(new Main);
			__factory.start();
			__factory.wait();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			retval = 1;
		}
		return retval;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
