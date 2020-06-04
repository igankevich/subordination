#ifndef SUBORDINATION_CORE_PARALLEL_PIPELINE_HH
#define SUBORDINATION_CORE_PARALLEL_PIPELINE_HH

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <subordination/core/pipeline_base.hh>

namespace sbn {

    class parallel_pipeline: public pipeline {

    private:
        struct compare_time {
            inline bool operator()(const kernel* lhs, const kernel* rhs) const noexcept {
                return lhs->at() > rhs->at();
            }
        };

    private:
        using kernel_queue = std::queue<kernel*>;
        using kernel_queue_array = std::vector<kernel_queue>;
        using kernel_priority_queue =
            std::priority_queue<kernel*,std::vector<kernel*>,compare_time>;
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = std::condition_variable;
        using semaphore_array = std::vector<semaphore_type>;
        using thread_type = std::thread;
        using thread_array = std::vector<thread_type>;

    private:
        /// Same mutex for all kernel queues.
        mutex_type _mutex;
        /// Upstream kernels.
        kernel_queue _upstream_kernels;
        thread_array _upstream_threads;
        semaphore_type _upstream_semaphore;
        /// Kernels that are scheduled to be executed at specific point of time.
        kernel_priority_queue _timer_kernels;
        thread_type _timer_thread;
        semaphore_type _timer_semaphore;
        /// Per-thread queue for downstream kernels.
        kernel_queue_array _downstream_kernels;
        thread_array _downstream_threads;
        semaphore_array _downstream_semaphores;
        /// Kernels that threw an exception are sent to this pipeline.
        pipeline* _error_pipeline = nullptr;

    public:

        inline parallel_pipeline(): parallel_pipeline(1u) {}

        inline explicit parallel_pipeline(unsigned num_upstream_threads):
        _upstream_threads(num_upstream_threads) {}

        ~parallel_pipeline() = default;
        parallel_pipeline(parallel_pipeline&&) = delete;
        parallel_pipeline& operator=(parallel_pipeline&&) = delete;
        parallel_pipeline(const parallel_pipeline&) = delete;
        parallel_pipeline& operator=(const parallel_pipeline&) = delete;

        inline void send(kernel* k) override {
            if (k->moves_downstream()) { this->send_downstream(k); }
            else if (k->scheduled()) { this->send_timer(k); }
            else { this->send_upstream(k); }
        }

        inline void send(kernel** kernels, size_t n) {
            bool notify_upstream = false, notify_timer = false;
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) {
                auto* k = kernels[i];
                #if defined(SBN_DEBUG)
                this->log("send _", *k);
                #endif
                if (k->moves_downstream()) {
                    const auto n = k->hash() % this->_downstream_kernels.size();
                    this->_downstream_kernels[n].emplace(k);
                    if (this->_downstream_threads.empty()) {
                        notify_upstream = true;
                    } else {
                        this->_downstream_semaphores[n].notify_one();
                    }
                } else if (k->scheduled()) {
                    this->_timer_kernels.emplace(k), notify_timer = true;
                } else {
                    this->_upstream_kernels.emplace(k), notify_upstream = true;
                }
            }
            if (notify_upstream) { this->_upstream_semaphore.notify_all(); }
            if (notify_timer) { this->_timer_semaphore.notify_one(); }
        }

        inline void send_upstream(kernel* k) {
            #if defined(SBN_DEBUG)
            this->log("send _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_upstream_kernels.emplace(k);
            this->_upstream_semaphore.notify_one();
        }

        inline void send_downstream(kernel* k) {
            #if defined(SBN_DEBUG)
            this->log("send _", *k);
            #endif
            lock_type lock(this->_mutex);
            const auto i = k->hash() % this->_downstream_kernels.size();
            this->_downstream_kernels[i].emplace(k);
            if (this->_downstream_threads.empty()) {
                this->_upstream_semaphore.notify_all();
            } else {
                this->_downstream_semaphores[i].notify_one();
            }
        }

        inline void send_timer(kernel* k) {
            #if defined(SBN_DEBUG)
            this->log("send _", *k);
            #endif
            this->_timer_kernels.emplace(k);
            this->_timer_semaphore.notify_one();
        }

        void start();
        void stop();
        void wait();
        void clear();

        void num_downstream_threads(size_t n);

        inline size_t num_downstream_threads() const noexcept {
            return this->_downstream_threads.size();
        }

        void num_upstream_threads(size_t n);

        inline size_t num_upstream_threads() const noexcept {
            return this->_upstream_threads.size();
        }

        inline void error_pipeline(pipeline* rhs) noexcept {
            this->_error_pipeline = rhs;
        }

    private:
        void upstream_loop(kernel_queue& downstream_queue);
        void timer_loop();
        void downstream_loop(kernel_queue& queue, semaphore_type& semaphore);

    };

}

#endif // vim:filetype=cpp
