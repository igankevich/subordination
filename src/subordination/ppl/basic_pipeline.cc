#include <subordination/ppl/basic_pipeline.hh>

#include <future>

#include <unistdx/base/log_message>

namespace {

    std::promise<int> return_value;

}

void
sbn::graceful_shutdown(int ret) {
    try {
        return_value.set_value(ret);
    } catch (const std::future_error& err) {
        sys::log_message(__func__, err.what());
    }
}

int
sbn::wait_and_return() {
    return return_value.get_future().get();
}

void sbn::basic_pipeline::start() {
    this->setstate(pipeline_state::started);
    auto thread_no = this->_number;
    for (auto& thr : this->_threads) {
        thr = std::thread(
            [this,thread_no] () {
                try {
                    sys::this_process::name(this->_name);
                } catch (...) {
                }
                this_thread::name = this->_name;
                this_thread::number = thread_no;
                this->run(&this_thread::context);
            });
        ++thread_no;
    }
}

void sbn::basic_pipeline::wait() {
    for (auto& thr : this->_threads) {
        if (thr.joinable()) { thr.join(); }
    }
}

void sbn::basic_pipeline::collect_kernels(kernel_sack&) {}
