#ifndef SUBORDINATION_PPL_BASIC_PIPELINE_HH
#define SUBORDINATION_PPL_BASIC_PIPELINE_HH

#include <cassert>
#include <queue>
#include <thread>
#include <vector>

#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/ipc/process>
#include <unistdx/ipc/thread_semaphore>
#include <unistdx/util/system>

#include <subordination/base/container_traits.hh>
#include <subordination/base/queue_popper.hh>
#include <subordination/base/queue_pusher.hh>
#include <subordination/kernel/kernel_type.hh>
#include <subordination/ppl/pipeline_base.hh>

namespace sbn {

    void graceful_shutdown(int ret);
    int wait_and_return();

}
#endif // vim:filetype=cpp
