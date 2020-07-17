#include <gtest/gtest.h>

#include <unistdx/base/log_message>

#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/parallel_pipeline.hh>

sbn::parallel_pipeline local;

template <class ... Args> inline void
message(const Args& ... args) {
    sys::log_message("test", args ...);
}

class Sleepy_kernel: public sbn::kernel {

private:
    size_t _serial_number = 0;

public:

    inline size_t serial_number() const noexcept { return this->_serial_number; }
    inline void serial_number(size_t rhs) noexcept { this->_serial_number = rhs; }

    void act() override {
        using namespace std::chrono;
        const auto now = sbn::kernel::clock_type::now();
        const auto delta = duration_cast<milliseconds>(now-at()).count();
        message("#_ wakes up _ms later than scheduled", this->_serial_number, delta);
        return_to_parent(sbn::exit_code::success);
        local.send_downstream(std::move(this_ptr()));
    }

};

class Main: public sbn::kernel {

private:
    size_t _last_serial_number = 0;
    size_t _nkernels;
    std::chrono::milliseconds _period;

public:

    inline explicit
    Main(size_t nkernels, std::chrono::milliseconds period):
    _nkernels(nkernels),
    _period(period)
    {}

    void act() override {
        std::vector<sbn::kernel_ptr> kernels(_nkernels);
        // send kernels in inverse chronological order
        for (size_t i=0; i<this->_nkernels; ++i) {
            kernels[i] = new_sleepy_kernel(this->_nkernels - i, this->_nkernels - i);
        }
        local.send(std::move(kernels));
    }

    void react(sbn::kernel_ptr&& child) override {
        auto k = sbn::pointer_dynamic_cast<Sleepy_kernel>(std::move(child));
        EXPECT_EQ(k->serial_number(), _last_serial_number+1)
            << "Invalid order of scheduled kernels";
        ++_last_serial_number;
        --this->_nkernels;
        if (this->_nkernels == 0) {
            return_to_parent(sbn::exit_code::success);
            local.send_downstream(std::move(this_ptr()));
        }
    }

private:

    inline sbn::kernel_ptr
    new_sleepy_kernel(int delay, int serial_number) {
        auto k = sbn::make_pointer<Sleepy_kernel>();
        k->after(delay * this->_period);
        k->parent(this);
        k->serial_number(serial_number);
        return k;
    }

};

TEST(TimerServerTest, All) {
    sbn::install_error_handler();
    local.start();
    local.send(sbn::make_pointer<Main>(10, std::chrono::milliseconds(500)));
    EXPECT_EQ(0, sbn::wait_and_return());
    local.stop();
    local.wait();
    sbn::kernel_sack sack;
    local.clear(sack);
}
