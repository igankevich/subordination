#ifndef SUBORDINATION_PPL_TIMER_PIPELINE_HH
#define SUBORDINATION_PPL_TIMER_PIPELINE_HH

#include <subordination/kernel/act.hh>
#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/compare_time.hh>
#include <unistdx/ipc/semaphore>

namespace sbn {

    namespace bits {

        using priority_queue =
            std::priority_queue<kernel*, std::vector<kernel*>, Compare_time<kernel>>;

        using timer_pipeline_base = basic_pipeline<
            priority_queue,
            priority_queue_traits<priority_queue>>;

    }

    class timer_pipeline: public bits::timer_pipeline_base {

    private:
        using base_pipeline = bits::timer_pipeline_base;

    public:
        using typename base_pipeline::mutex_type;
        using typename base_pipeline::lock_type;
        using typename base_pipeline::sem_type;
        using typename base_pipeline::traits_type;

        inline
        timer_pipeline(timer_pipeline&& rhs) noexcept:
        base_pipeline(std::move(rhs))
        {}

        inline
        timer_pipeline() noexcept:
        base_pipeline(1u)
        {}

        timer_pipeline(const timer_pipeline&) = delete;
        timer_pipeline& operator=(const timer_pipeline&) = delete;
        timer_pipeline& operator=(timer_pipeline&&) = delete;

        ~timer_pipeline() = default;

    protected:
        void
        do_run() override;

    private:
        inline void
        wait_until_kernel_arrives(lock_type& lock) {
            this->_semaphore.wait(
                lock,
                [this] () {
                    return this->has_stopped() || !this->_kernels.empty();
                }
            );
        }

        inline bool
        wait_until_kernel_is_ready(lock_type& lock, kernel* k) {
            return this->_semaphore.wait_until(
                lock,
                k->at(),
                [this] { return this->has_stopped(); }
            );
        }

    };

}

#endif // vim:filetype=cpp
