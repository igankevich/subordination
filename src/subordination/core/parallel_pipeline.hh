#ifndef SUBORDINATION_CORE_PARALLEL_PIPELINE_HH
#define SUBORDINATION_CORE_PARALLEL_PIPELINE_HH

#include <condition_variable>
#include <mutex>
#include <queue>

#include <subordination/bits/contracts.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/thread_pool.hh>

namespace sbn {

    class parallel_pipeline: public pipeline {

    public:
        struct properties {
            sys::cpu_set upstream_cpus;
            sys::cpu_set downstream_cpus;
            sys::cpu_set timer_cpus;
            sys::cpu_set kernel_cpus;
            unsigned num_downstream_threads = 0;
            unsigned num_upstream_threads;

            inline properties(): properties{sys::this_process::cpu_affinity()} {}

            inline explicit properties(unsigned nthreads):
            properties{sys::this_process::cpu_affinity(), nthreads} {}

            inline explicit properties(const sys::cpu_set& cpus):
            properties{cpus,static_cast<unsigned int>(cpus.count())} {}

            inline explicit properties(const sys::cpu_set& cpus, unsigned nthreads):
            upstream_cpus{cpus}, downstream_cpus{cpus}, timer_cpus{cpus}, kernel_cpus{cpus},
            num_upstream_threads{nthreads} {}

            bool set(const char* key, const std::string& value);
        };

    private:
        struct compare_time {
            inline bool operator()(const kernel_ptr& a, const kernel_ptr& b) const noexcept {
                return a->at() > b->at();
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
        using thread_init_type = std::function<void(size_t)>;

    private:
        /// Same mutex for all kernel queues.
        mutex_type _mutex;
        /// Upstream kernels.
        kernel_queue _upstream_kernels;
        thread_pool _upstream_threads;
        semaphore_type _upstream_semaphore;
        /// Kernels that are scheduled to be executed at specific point of time.
        kernel_priority_queue _timer_kernels;
        thread_pool _timer_threads;
        semaphore_type _timer_semaphore;
        /// Per-thread queue for downstream kernels.
        kernel_queue_array _downstream_kernels;
        thread_pool _downstream_threads;
        semaphore_array _downstream_semaphores;
        /// Per-kernel threads for 'new-thread' kernels.
        thread_pool _kernel_threads;
        /// Kernels that threw an exception are sent to this pipeline.
        pipeline* _error_pipeline = nullptr;
        /// Function that is called in each new thread.
        thread_init_type _thread_init;

    public:

        inline parallel_pipeline(): parallel_pipeline{1u} {}

        inline explicit parallel_pipeline(unsigned num_upstream_threads):
        parallel_pipeline{properties{num_upstream_threads}} {}

        explicit parallel_pipeline(const properties& p):
        _upstream_threads(p.num_upstream_threads),
        _downstream_kernels(p.num_downstream_threads),
        _downstream_threads(p.num_downstream_threads),
        _downstream_semaphores(p.num_downstream_threads) {
            this->_upstream_threads.cpus(p.upstream_cpus);
            this->_downstream_threads.cpus(p.downstream_cpus);
            this->_timer_threads.cpus(p.timer_cpus);
            this->_kernel_threads.cpus(p.kernel_cpus);
        }

        ~parallel_pipeline() = default;
        parallel_pipeline(parallel_pipeline&&) = delete;
        parallel_pipeline& operator=(parallel_pipeline&&) = delete;
        parallel_pipeline(const parallel_pipeline&) = delete;
        parallel_pipeline& operator=(const parallel_pipeline&) = delete;

        inline void send(kernel_ptr&& k) override {
            Expects(k.get());
            if (k->phase() == kernel::phases::downstream) {
                this->send_downstream(std::move(k));
            } else if (k->scheduled()) {
                this->send_timer(std::move(k));
            } else if (k->new_thread()) {
                this->send_thread(std::move(k));
            } else {
                this->send_upstream(std::move(k));
            }
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
                } else if (k->new_thread()) {
                    make_thread(std::move(k));
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

        inline void send_thread(kernel_ptr&& k) {
            #if defined(SBN_DEBUG)
            this->log("thread _", *k);
            #endif
            lock_type lock(this->_mutex);
            make_thread(std::move(k));
        }

        void start();
        void stop();
        void wait();
        void clear(kernel_sack& sack);

        inline void error_pipeline(pipeline* rhs) noexcept {
            this->_error_pipeline = rhs;
        }

        inline pipeline* error_pipeline() const noexcept {
            return this->_error_pipeline;
        }

        inline void thread_init(thread_init_type rhs) { this->_thread_init = rhs; }

        void num_downstream_threads(size_t n);
        void num_upstream_threads(size_t n);

    private:
        void upstream_loop(kernel_queue& downstream_queue);
        void upstream_start(size_t num_threads);
        void timer_loop();
        void timer_start();
        void downstream_loop(kernel_queue& queue, semaphore_type& semaphore);
        void downstream_start(size_t num_threads);
        void kernel_loop(kernel_ptr kernel);

        inline void make_thread(kernel_ptr&& k) {
            this->_kernel_threads.emplace_back(
                [this] (kernel_ptr&& k) {
                    sys::this_process::cpu_affinity(this->_kernel_threads.cpus());
                    kernel_loop(std::move(k));
                },
                std::move(k));
        }

    };

}

#endif // vim:filetype=cpp
