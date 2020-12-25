#ifndef SUBORDINATION_CORE_PARALLEL_PIPELINE_HH
#define SUBORDINATION_CORE_PARALLEL_PIPELINE_HH

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <subordination/bits/contracts.hh>
#include <subordination/core/pipeline_base.hh>

namespace sbn {

    class parallel_pipeline: public pipeline {

    private:
        struct compare_time {
            inline bool operator()(const kernel_ptr& lhs, const kernel_ptr& rhs) const noexcept {
                return lhs->at() > rhs->at();
            }
        };

    private:
        using kernel_queue = std::queue<kernel_ptr>;
        using kernel_queue_array = std::vector<kernel_queue>;
        using kernel_priority_queue =
            std::priority_queue<kernel_ptr,std::vector<kernel_ptr>,compare_time>;
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;
        using semaphore_type = std::condition_variable;
        using semaphore_array = std::vector<semaphore_type>;
        // TODO replace with sys::process
        using thread_type = std::thread;
        using thread_array = std::vector<thread_type>;
        using thread_init_type = std::function<void(size_t)>;

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
        /// Function that is called in each new thread.
        thread_init_type _thread_init;

    public:

        inline parallel_pipeline(): parallel_pipeline(1u) {}

        inline explicit parallel_pipeline(unsigned num_upstream_threads):
        _upstream_threads(num_upstream_threads) {}

        ~parallel_pipeline() = default;
        parallel_pipeline(parallel_pipeline&&) = delete;
        parallel_pipeline& operator=(parallel_pipeline&&) = delete;
        parallel_pipeline(const parallel_pipeline&) = delete;
        parallel_pipeline& operator=(const parallel_pipeline&) = delete;

        inline void send(kernel_ptr&& k) override {
            Expects(k.get());
            if (k->phase() == kernel::phases::downstream) {
                this->send_downstream(std::move(k));
            } else if (k->scheduled()) { this->send_timer(std::move(k)); }
            else { this->send_upstream(std::move(k)); }
        }

        inline void send(kernel_ptr_array&& kernels) {
            bool notify_upstream = false, notify_timer = false;
            const auto n = kernels.size();
            lock_type lock(this->_mutex);
            for (size_t i=0; i<n; ++i) {
                auto& k = kernels[i];
                Assert(k.get());
                if (k->phase() == kernel::phases::downstream) {
                    #if defined(SBN_DEBUG)
                    this->log("downstream _", *k);
                    #endif
                    // TODO Save the index of the pipeline that executed upstream phase
                    // of this kernel and then execute downstream phase by the same
                    // pipeline.
                    const auto num_upstream_threads = this->_upstream_threads.size();
                    const auto num_downstream_threads = this->_downstream_threads.size();
                    const auto size = std::max(num_upstream_threads,num_downstream_threads);
                    const auto n = k->hash() % size;
                    this->_downstream_kernels[n].emplace(std::move(k));
                    if (this->_downstream_threads.empty()) {
                        notify_upstream = true;
                    } else {
                        this->_downstream_semaphores[n].notify_one();
                    }
                } else if (k->scheduled()) {
                    #if defined(SBN_DEBUG)
                    this->log("schedule _", *k);
                    #endif
                    this->_timer_kernels.emplace(std::move(k)), notify_timer = true;
                } else {
                    #if defined(SBN_DEBUG)
                    this->log("upstream _", *k);
                    #endif
                    this->_upstream_kernels.emplace(std::move(k)), notify_upstream = true;
                }
            }
            if (notify_upstream) { this->_upstream_semaphore.notify_all(); }
            if (notify_timer) { this->_timer_semaphore.notify_one(); }
        }

        inline void send_upstream(kernel_ptr&& k) {
            Expects(k.get());
            #if defined(SBN_DEBUG)
            this->log("upstream _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_upstream_kernels.emplace(std::move(k));
            this->_upstream_semaphore.notify_one();
        }

        inline void send_downstream(kernel_ptr&& k) {
            Expects(k.get());
            #if defined(SBN_DEBUG)
            this->log("downstream _", *k);
            #endif
            lock_type lock(this->_mutex);
            const auto num_upstream_threads = this->_upstream_threads.size();
            const auto num_downstream_threads = this->_downstream_threads.size();
            const auto size = std::max(num_upstream_threads,num_downstream_threads);
            const auto i = k->hash() % size;
            this->_downstream_kernels[i].emplace(std::move(k));
            if (this->_downstream_threads.empty()) {
                this->_upstream_semaphore.notify_all();
            } else {
                this->_downstream_semaphores[i].notify_one();
            }
        }

        inline void send_timer(kernel_ptr&& k) {
            Expects(k.get());
            #if defined(SBN_DEBUG)
            this->log("schedule _", *k);
            #endif
            lock_type lock(this->_mutex);
            this->_timer_kernels.emplace(std::move(k));
            this->_timer_semaphore.notify_one();
        }

        void start();
        void stop();
        void wait();
        void clear(kernel_sack& sack);

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

        inline pipeline* error_pipeline() const noexcept {
            return this->_error_pipeline;
        }

        inline void thread_init(thread_init_type rhs) { this->_thread_init = rhs; }

    private:
        void upstream_loop(kernel_queue& downstream_queue);
        void timer_loop();
        void downstream_loop(kernel_queue& queue, semaphore_type& semaphore);

    };

}

#endif // vim:filetype=cpp
