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
#include <subordination/base/thread_name.hh>
#include <subordination/kernel/kernel_type.hh>
#include <subordination/ppl/pipeline_base.hh>
#include <subordination/ppl/thread_context.hh>

namespace sbn {

    void graceful_shutdown(int ret);
    int wait_and_return();

    class basic_pipeline: public pipeline {

    protected:
        using kernel_sack = std::vector<std::unique_ptr<kernel>>;

    protected:
        std::vector<std::thread> _threads;

    public:
        basic_pipeline() = default;

        inline explicit
        basic_pipeline(unsigned concurrency):
        _threads(std::max(1u, concurrency)) {}

        inline
        ~basic_pipeline() {
            // ensure that kernels inserted without starting
            // a pipeline are deleted
            kernel_sack sack;
            this->collect_kernels(sack);
        }

        basic_pipeline(basic_pipeline&&) = delete;
        basic_pipeline& operator=(basic_pipeline&&) = delete;
        basic_pipeline(const basic_pipeline&) = delete;
        basic_pipeline& operator=(const basic_pipeline&) = delete;

        void start();
        inline void stop() { this->setstate(pipeline_state::stopped); }
        void wait();

        inline unsigned concurrency() const noexcept { return this->_threads.size(); }

    protected:

        virtual void
        do_run() = 0;

        virtual void
        run(Thread_context* context) {
            if (context) {
                Thread_context_guard lock(*context);
                context->register_thread();
            }
            this->do_run();
            if (context) {
                this->collect_kernels(*context);
            }
        }

        virtual void collect_kernels(kernel_sack& sack);

    private:

        void
        collect_kernels(Thread_context& context) {
            // Recursively collect kernel pointers to the sack
            // and delete them all at once. Collection process
            // is fully serial to prevent multiple deletions
            // and access to unitialised values.
            Thread_context_guard lock(context);
            //std::clog << "global_barrier #1" << std::endl;
            context.wait(lock);
            //std::clog << "global_barrier #1 end" << std::endl;
            kernel_sack sack;
            this->collect_kernels(sack);
            // simple barrier for all threads participating in deletion
            //std::cout << "global_barrier #2" << std::endl;
            //context.global_barrier(lock);
            //std::cout << "global_barrier #2 end" << std::endl;
            // destructors of scoped variables
            // will destroy all kernels automatically
        }

    };

}
#endif // vim:filetype=cpp
