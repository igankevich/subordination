#include <factory/api.hh>
#include <factory/ppl/cpu_server.hh>
#include <factory/ppl/server_guard.hh>
#include <factory/ppl/timer_server.hh>
#include <factory/base/error_handler.hh>

#include <gtest/gtest.h>

factory::CPU_server<factory::Kernel> local_server;
factory::Timer_server<factory::Kernel> timer_server;

struct Sleepy_kernel: public factory::Kernel {

	void pos(size_t p) { _pos = p; }
	size_t pos() const { return _pos; }

	void act() override {
		using namespace std::chrono;
		const auto now = factory::Kernel::Clock::now();
		const auto at = this->at();
		const auto delta = duration_cast<nanoseconds>(now-at).count();
		std::clog << '#' << _pos << " wakes up "
			<< delta << "ns later than scheduled\n";
		factory::commit(local_server, this);
	}

private:

	size_t _pos = 0;

};

struct Main: public factory::Kernel {

	Main(size_t nkernels, std::chrono::milliseconds period):
	_nkernels(nkernels),
	_period(period)
	{}

	void
	act() override {
		std::vector<factory::Kernel*> kernels(_nkernels);
		// send kernels in inverse chronological order
		for (size_t i=0; i<_nkernels; ++i) {
			kernels[i] = new_sleepy_kernel(
				this->_nkernels - i,
				this->_nkernels - i
			);
		}
		timer_server.send(kernels.data(), kernels.size());
	}

	void
	react(factory::Kernel* child) override {
		Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
		EXPECT_EQ(k->pos(), last_pos+1) << "Invalid order of scheduled kernels";
		++last_pos;
		--this->_nkernels;
		if (this->_nkernels == 0) {
			factory::commit(local_server, this);
		}
	}

private:

	factory::Kernel*
	new_sleepy_kernel(int delay, int pos) {
		Sleepy_kernel* kernel = new Sleepy_kernel;
		kernel->after(delay * _period);
		kernel->parent(this);
		kernel->pos(pos);
		return kernel;
	}

	size_t last_pos = 0;
	size_t _nkernels;
	std::chrono::milliseconds _period;

};

TEST(TimerServerTest, All) {
	factory::install_error_handler();
	factory::start_all(local_server, timer_server);
	local_server.send(new Main(10, std::chrono::milliseconds(500)));
	EXPECT_EQ(0, factory::wait_and_return());
	factory::stop_all(local_server, timer_server);
	factory::wait_all(local_server, timer_server);
}
