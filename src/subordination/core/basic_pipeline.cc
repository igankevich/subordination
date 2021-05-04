#include <condition_variable>
#include <mutex>

#include <unistdx/base/log_message>
#include <unistdx/ipc/process>
#include <unistdx/system/error>

#include <subordination/core/basic_pipeline.hh>

namespace {
    int return_value = 0;
    std::mutex return_mutex;
    std::condition_variable return_cv;
}

void sbn::exit(int ret) {
    /*
    #if defined(UNISTDX_HAVE_PRCTL)
    std::string nm(16, '\0');
    ::prctl(PR_GET_NAME, nm.data());
    #endif
    #if defined(SBN_DEBUG)
    #endif
    sys::log_message(nm.data(), "exit _ stack trace\n_", ret, sys::stack_trace());
    */
    std::unique_lock<std::mutex> lock(return_mutex);
    return_value = ret;
    return_cv.notify_all();
}

int sbn::wait_and_return() {
    std::unique_lock<std::mutex> lock(return_mutex);
    return_cv.wait(lock);
    return return_value;
}
