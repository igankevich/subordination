#ifndef SUBORDINATION_PPL_TIMER_PIPELINE_HH
#define SUBORDINATION_PPL_TIMER_PIPELINE_HH

#include <condition_variable>
#include <mutex>

#include <subordination/kernel/act.hh>
#include <subordination/ppl/basic_pipeline.hh>
#include <unistdx/ipc/semaphore>

namespace sbn {

    namespace bits {

        struct compare_time {
            inline bool
            operator()(const kernel* lhs, const kernel* rhs) const noexcept {
                return lhs->at() > rhs->at();
            }
        };

    }

    class timer_pipeline: public basic_pipeline {

    private:
        using kernel_queue =
            std::priority_queue<kernel*,std::vector<kernel*>,bits::compare_time>;
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = std::condition_variable;

    private:
        kernel_queue _kernels;
        mutex_type _mutex;
        semaphore_type _semaphore;

    public:

        inline timer_pipeline(): basic_pipeline(1u) {}

        timer_pipeline(timer_pipeline&&) = delete;
        timer_pipeline(const timer_pipeline&) = delete;
        timer_pipeline& operator=(const timer_pipeline&) = delete;
        timer_pipeline& operator=(timer_pipeline&&) = delete;

        ~timer_pipeline() = default;

        inline void send(kernel* k) override {
            lock_type lock(this->_mutex);
            this->_kernels.emplace(k);
            this->_semaphore.notify_one();
        }

        inline void send(kernel** kernels, size_t n) {
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) { this->_kernels.emplace(kernels[i]); }
            this->_semaphore.notify_all();
        }

    protected:
        void do_run() override;
        void collect_kernels(kernel_sack& sack) override;

    };

}

#endif // vim:filetype=cpp
