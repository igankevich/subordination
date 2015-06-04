#include <factory/factory.hh>

using namespace factory;

struct Sleepy_kernel: public Kernel {

	void pos(int p) { position = p; }
	int pos() const { return position; }

	void act() {
		Logger<Level::TEST>() << "Waking up at "
			<< current_time_nano() << ", scheduled at "
			<< this->at().time_since_epoch().count() << std::endl;
		commit(the_server());
	}

	int position = 0;
};

struct Main: public Kernel {
	void act() {
		// send kernels in reversed chronological order
		for (int i=0; i<NUM_KERNELS; ++i) {
			send_sleepy_kernel(NUM_KERNELS - i, NUM_KERNELS - i);
		}
	}
	void react(Kernel* child) {
		Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
		if (last_pos + 1 != k->pos()) {
			throw std::runtime_error("Invalid order of timed kernels.");
		}
		++last_pos;
		if (++count == NUM_KERNELS) {
			commit(the_server());
		}
	}

private:

	void send_sleepy_kernel(int delay, int pos) {
		Sleepy_kernel* kernel = new Sleepy_kernel;
		kernel->after(std::chrono::milliseconds(delay*500));
		kernel->parent(this);
		kernel->pos(pos);
		timer_server()->send(kernel);
	}

	size_t count = 0;
	size_t last_pos = 0;

	static const size_t NUM_KERNELS = 10;
};

struct App {
	int run(int argc, char* argv[]) {
		Logger<Level::TEST>() << "sizeof(time_point) == " << sizeof(Kernel::Clock::time_point) << std::endl;
		Logger<Level::TEST>() << "sizeof(duration) == " << sizeof(Kernel::Clock::duration) << std::endl;
		int retval = 0;
		try {
			the_server()->add_cpu(0);
			Logger<Level::TEST>() << "Starting at " << current_time_nano() << std::endl;
			__factory.start();
			the_server()->send(new Main);
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
