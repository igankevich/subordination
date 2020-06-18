#ifndef SUBORDINATION_CORE_BASIC_PIPELINE_HH
#define SUBORDINATION_CORE_BASIC_PIPELINE_HH

#include <cassert>
#include <queue>
#include <thread>
#include <vector>

#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/ipc/process>
#include <unistdx/ipc/thread_semaphore>
#include <unistdx/util/system>

#include <subordination/core/kernel_type.hh>
#include <subordination/core/pipeline_base.hh>

namespace sbn {

    void exit(int ret);

    inline void exit(kernel* k) {
        const auto ret = static_cast<int>(k->return_code());
        delete k;
        exit(ret);
    }

    int wait_and_return();

}
#endif // vim:filetype=cpp
