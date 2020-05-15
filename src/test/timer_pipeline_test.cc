#include <subordination/api.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#include <subordination/ppl/timer_pipeline.hh>
#include <subordination/base/error_handler.hh>

#include <gtest/gtest.h>

using namespace sbn;

struct Sleepy_kernel: public sbn::kernel {

    void pos(size_t p) { _pos = p; }
    size_t pos() const { return _pos; }

    void act() override {
        using namespace std::chrono;
        const auto now = sbn::kernel::clock_type::now();
        const auto at = this->at();
        const auto delta = duration_cast<nanoseconds>(now-at).count();
        std::clog << '#' << _pos << " wakes up "
            << delta << "ns later than scheduled\n";
        commit<Local>(this);
    }

private:

    size_t _pos = 0;

};

struct Main: public sbn::kernel {

    Main(size_t nkernels, std::chrono::milliseconds period):
    _nkernels(nkernels),
    _period(period)
    {}

    void
    act() override {
        std::vector<sbn::kernel*> kernels(_nkernels);
        // send kernels in inverse chronological order
        for (size_t i=0; i<this->_nkernels; ++i) {
            kernels[i] = new_sleepy_kernel(
                this->_nkernels - i,
                this->_nkernels - i
            );
        }
        sbn::factory.send_timer(kernels.data(), kernels.size());
    }

    void
    react(sbn::kernel* child) override {
        Sleepy_kernel* k = dynamic_cast<Sleepy_kernel*>(child);
        EXPECT_EQ(k->pos(), last_pos+1) << "Invalid order of scheduled kernels";
        ++last_pos;
        --this->_nkernels;
        if (this->_nkernels == 0) {
            commit<Local>(this);
        }
    }

private:

    sbn::kernel*
    new_sleepy_kernel(int delay, int pos) {
        Sleepy_kernel* k = new Sleepy_kernel;
        k->after(delay * _period);
        k->parent(this);
        k->pos(pos);
        return k;
    }

    size_t last_pos = 0;
    size_t _nkernels;
    std::chrono::milliseconds _period;

};

TEST(TimerServerTest, All) {
    sbn::install_error_handler();
    factory_guard g;
    send<Local>(new Main(10, std::chrono::milliseconds(500)));
    EXPECT_EQ(0, sbn::wait_and_return());
}
