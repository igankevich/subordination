#include <subordination/core/basic_pipeline.hh>

#include <future>

#include <unistdx/base/log_message>
#include <unistdx/util/backtrace>

namespace {

    std::promise<int> return_value;

}

void
sbn::graceful_shutdown(int ret) {
    try {
        return_value.set_value(ret);
    } catch (const std::future_error& err) {
        sys::log_message(__func__, err.what());
        sys::backtrace(2);
    }
}

int
sbn::wait_and_return() {
    return return_value.get_future().get();
}
