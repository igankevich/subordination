#include <cstdlib>

#include <valgrind/config.hh>

#include <gtest/gtest.h>

#include <unistdx/base/log_message>
#include <unistdx/ipc/process>

#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/parallel_pipeline.hh>

template <class ... Args> inline void
message(const Args& ... args) {
    sys::log_message("test", args ...).out().flush();
}

constexpr const int num_kernels = 100;
sbn::parallel_pipeline local{1};

class Child: public sbn::kernel {
public:
    void act() override {
        message("child::act");
        return_to_parent(sbn::exit_code::success);
        local.send(std::move(this_ptr()));
    }
};

class Main: public sbn::kernel {

private:
    int _counter = 0;
    std::mutex _mutex;
    std::condition_variable _semaphore;

public:

    Main() {
        setf(sbn::kernel::flag::new_thread);
    }

    void act() override {
        for (int i=0; i<num_kernels; ++i) {
            auto child = sbn::make_pointer<Child>();
            child->parent(this);
            local.send(std::move(child));
        }
        std::unique_lock<std::mutex> lock(this->_mutex);
        this->_semaphore.wait(lock, [this] () { return this->_counter == num_kernels; });
        sbn::exit(0);
    }

    void react(sbn::kernel_ptr&&) override {
        message("main::react");
        std::unique_lock<std::mutex> lock(this->_mutex);
        ++this->_counter;
        this->_semaphore.notify_one();
    }

};

TEST(parallel_pipeline, _) {
    local.name("local");
    local.start();
    local.send(sbn::make_pointer<Main>());
    auto retval = sbn::wait_and_return();
    EXPECT_EQ(0, retval);
    local.stop();
    local.wait();
    sbn::kernel_sack sack;
    local.clear(sack);
}

int main(int argc, char* argv[]) {
    SBN_SKIP_IF_RUNNING_ON_VALGRIND();
    sbn::install_error_handler();
    ::testing::InitGoogleTest(&argc, argv);
    sys::this_process::ignore_signal(sys::signal::broken_pipe);
    return RUN_ALL_TESTS();
}
