#include "libfactory.cc"

using namespace factory;
using factory::components::Error;

struct Sleepy_kernel: public Kernel {

	void pos(int p) { position = p; }
	int pos() const { return position; }

	void act() {
		const auto now = current_time_nano();
		const auto at = this->at().time_since_epoch().count();
		Logger<Level::TEST>()
			<< "Waking up at " << now
			<< ", scheduled at " << at
			<< ", delta=" << std::abs(now-at)
			<< std::endl;
		commit(the_server());
	}

	int position = 0;
};

struct Main: public Kernel {
	void act() {
		std::vector<Kernel*> kernels(NUM_KERNELS);
		// send kernels in reversed chronological order
		for (int i=0; i<NUM_KERNELS; ++i) {
			kernels[i] = new_sleepy_kernel(NUM_KERNELS - i, NUM_KERNELS - i);
		}
		timer_server()->send(kernels.data(), kernels.size());
	}
	void react(Kernel* child) {
		Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
		if (last_pos + 1 != k->pos()) {
			std::stringstream msg;
			msg << "Invalid order of timed kernels: pos="
				<< k->pos() << ",valid_pos=" << last_pos + 1;
			throw Error(msg.str(),
				__FILE__, __LINE__, __func__);
		}
		++last_pos;
		if (++count == NUM_KERNELS) {
			commit(the_server());
		}
	}

private:

	Kernel*
	new_sleepy_kernel(int delay, int pos) {
		Sleepy_kernel* kernel = new Sleepy_kernel;
		kernel->after(std::chrono::milliseconds(delay*500));
		kernel->parent(this);
		kernel->pos(pos);
		return kernel;
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
