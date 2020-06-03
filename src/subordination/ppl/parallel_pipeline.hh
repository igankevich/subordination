#ifndef SUBORDINATION_PPL_PARALLEL_PIPELINE_HH
#define SUBORDINATION_PPL_PARALLEL_PIPELINE_HH

#include <subordination/ppl/basic_pipeline.hh>

namespace sbn {

    class parallel_pipeline: public basic_pipeline {

    private:
        using kernel_queue = std::queue<kernel*>;
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = std::condition_variable;

    private:
        kernel_queue _kernels;
        mutex_type _mutex;
        semaphore_type _semaphore;

    public:

        inline parallel_pipeline(): parallel_pipeline(1u) {}
        inline explicit parallel_pipeline(unsigned concurrency): basic_pipeline(concurrency) {}
        ~parallel_pipeline() = default;
        parallel_pipeline(parallel_pipeline&&) = delete;
        parallel_pipeline& operator=(parallel_pipeline&&) = delete;
        parallel_pipeline(const parallel_pipeline&) = delete;
        parallel_pipeline& operator=(const parallel_pipeline&) = delete;

        inline void send(kernel* k) override {
            #ifndef NDEBUG
            this->log("send _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_kernels.emplace(k);
            this->_semaphore.notify_one();
        }

        inline void send(kernel** kernels, size_t n) {
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) { this->_kernels.emplace(kernels[i]); }
            this->_semaphore.notify_all();
        }

        void stop();

    protected:

        void do_run() override;
        void collect_kernels(kernel_sack& sack) override;

    };

}

#endif // vim:filetype=cpp
