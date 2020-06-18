#include <subordination/core/basic_pipeline.hh>

#include <future>

#include <unistdx/base/log_message>
#include <unistdx/ipc/process>
#include <unistdx/util/backtrace>

namespace { std::promise<int> return_value; }

void sbn::exit(int ret) {
    try {
        #if defined(UNISTDX_HAVE_PRCTL)
        std::string nm(16, '\0');
        ::prctl(PR_GET_NAME, nm.data());
        #endif
        sys::log_message(nm.data(), "exit _", ret);
        return_value.set_value(ret);
    } catch (const std::future_error& err) {
        sys::log_message(__func__, err.what());
        sys::backtrace(2);
    }
}

int sbn::wait_and_return() {
    return return_value.get_future().get();
}
