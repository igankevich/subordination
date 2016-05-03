#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>

#include "test.hh"
#include "stdx/log.hh"

namespace factory {
	typedef std::chrono::nanoseconds::rep Time;
	typedef std::chrono::nanoseconds Nanoseconds;
	typedef typename std::make_signed<Time>::type Skew;

	static Time current_time_nano() {
		using namespace std::chrono;
		typedef std::chrono::steady_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
	}
}

namespace factory {

inline namespace timer_config {

	struct config {
		typedef components::CPU_server<config> local_server;
		typedef components::Timer_server<config> timer_server;
	};

}

}

using namespace factory;
using namespace factory::timer_config;
using factory::components::Error;

struct Sleepy_kernel: public Kernel {

	void pos(size_t p) { position = p; }
	size_t pos() const { return position; }

	void act(Server& this_server) {
		const auto now = current_time_nano();
		const auto at = this->at().time_since_epoch().count();
		std::cout
			<< "Waking up at " << now
			<< ", scheduled at " << at
			<< ", delta=" << now-at
			<< std::endl;
		commit(this_server.local_server());
	}

	size_t position = 0;
};

struct Main: public Kernel {
	typedef stdx::log<Main> this_log;
	Main(Server& this_server, int argc, char* argv[]) {}
	void act(Server& this_server) {
		std::vector<Kernel*> kernels(NUM_KERNELS);
		// send kernels in inverse chronological order
		for (size_t i=0; i<NUM_KERNELS; ++i) {
			kernels[i] = new_sleepy_kernel(NUM_KERNELS - i, NUM_KERNELS - i);
		}
		factory()->timer_server()->send(kernels.data(), kernels.size());
	}
	void react(Server& this_server, Kernel* child) {
		Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
		test::equal(k->pos(), last_pos+1, "Invalid order of timed kernels");
		++last_pos;
		if (++count == NUM_KERNELS) {
			commit(local_server());
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

int
main(int argc, char* argv[]) {
	using namespace factory;
	return factory_main<Main,config>(argc, argv);
}
