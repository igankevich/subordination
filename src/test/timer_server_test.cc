#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/algorithm.hh>
#include <factory/server_guard.hh>

#include "test.hh"
#include "stdx/log.hh"

factory::components::CPU_server<factory::Kernel> local_server;
factory::components::Timer_server<factory::Kernel> timer_server;

typedef std::chrono::nanoseconds::rep Time;
typedef std::chrono::nanoseconds Nanoseconds;
typedef typename std::make_signed<Time>::type Skew;

static Time
current_time_nano() {
	using namespace std::chrono;
	typedef std::chrono::system_clock Clock;
	return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
}

using factory::components::Error;

struct Sleepy_kernel: public factory::Kernel {

	void pos(size_t p) { position = p; }
	size_t pos() const { return position; }

	void act() override {
		const auto now = current_time_nano();
		const auto at = this->at().time_since_epoch().count();
		std::cout
			<< "Waking up at " << now
			<< ", scheduled at " << at
			<< ", delta=" << now-at
			<< std::endl;
		factory::commit(local_server, this);
	}

	size_t position = 0;
};

struct Main: public factory::Kernel {

	typedef stdx::log<Main> this_log;

	void
	act() override {
		std::vector<factory::Kernel*> kernels(NUM_KERNELS);
		// send kernels in inverse chronological order
		for (size_t i=0; i<NUM_KERNELS; ++i) {
			kernels[i] = new_sleepy_kernel(NUM_KERNELS - i, NUM_KERNELS - i);
		}
		timer_server.send(kernels.data(), kernels.size());
	}

	void
	react(factory::Kernel* child) override {
		Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
		test::equal(k->pos(), last_pos+1, "Invalid order of timed kernels");
		++last_pos;
		if (++count == NUM_KERNELS) {
			std::cout << "final commit" << std::endl;
			factory::commit(local_server, this);
		}
	}

private:

	factory::Kernel*
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

int
main(int argc, char* argv[]) {
	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(timer_server)> g2(timer_server);
	local_server.send(new Main);
	return factory::wait_and_return();
}
