#ifndef SUBORDINATION_DAEMON_PROCESS_PIPELINE_HH
#define SUBORDINATION_DAEMON_PROCESS_PIPELINE_HH

#include <deque>
#include <memory>
#include <unordered_map>

#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>

#include <subordination/core/application.hh>
#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/process_handler.hh>

namespace sbnd {

    enum class process_pipeline_event: sys::u8 {
        child_process_executed=1<<0,
        child_process_terminated=1<<1,
    };

    UNISTDX_FLAGS(process_pipeline_event);

    class process_pipeline_kernel: public sbn::kernel {

    private:
        sbn::application::id_type _application_id{};
        sys::process_status _status;
        process_pipeline_event _event{};

    public:
        inline sbn::application::id_type application_id() const noexcept {
            return this->_application_id;
        }
        inline void application_id(sbn::application::id_type rhs) noexcept {
            this->_application_id = rhs;
        }
        inline sys::process_status status() const noexcept { return this->_status; }
        inline void status(sys::process_status rhs) noexcept { this->_status = rhs; }
        inline process_pipeline_event event() const noexcept { return this->_event; }
        inline void event(process_pipeline_event rhs) noexcept { this->_event = rhs; }

    };

    class process_pipeline: public sbn::basic_socket_pipeline {

    public:
        struct properties: public sbn::basic_socket_pipeline::properties {
            size_t pipe_buffer_size;
            bool allow_root = false;
            bool interleave = false;

            inline properties():
            properties{sys::this_process::cpu_affinity(), sys::page_size()} {}

            inline explicit
            properties(const sys::cpu_set& cpus, size_t page_size, size_t multiple=16):
            sbn::basic_socket_pipeline::properties{cpus, page_size, multiple},
            pipe_buffer_size{page_size*multiple} {}

            bool set(const char* key, const std::string& value);
        };

    private:
        using connection_type = sbn::process_handler;
        using connection_ptr = std::shared_ptr<connection_type>;
        using application_id_type = sbn::application::id_type;
        using application_table = std::unordered_map<application_id_type,connection_ptr>;
        using app_iterator = typename application_table::iterator;
        using kernel_queue = std::deque<sbn::kernel_ptr>;

    private:
        std::thread _waiting_thread;
        application_table _jobs;
        sys::process_group _child_processes;
        pipeline* _unix{};
        size_t _pipe_buffer_size = 4096UL*16UL;
        /// How long a child process lives without receiving/sending kernels.
        duration _timeout;
        kernel_queue _outstanding_kernels;
        /** How long outstanding kernels can wait for the resources. When
        the time runs out, the kernel is sent back to the source cluster
        node. */
        duration _kernel_timeout;
        unsigned _max_threads = sys::thread_concurrency();
        /// Allow process execution as superuser/supergroup.
        bool _allowroot = true;
        /// Interleave kernels from different applications.
        bool _interleave = false;

    public:

        explicit process_pipeline(const properties& p);
        process_pipeline() = default;
        ~process_pipeline() = default;
        process_pipeline(const process_pipeline&) = delete;
        process_pipeline& operator=(const process_pipeline&) = delete;
        process_pipeline(process_pipeline&& rhs) = default;
        process_pipeline& operator=(process_pipeline&) = default;

        inline void
        add(const sbn::application& app) {
            lock_type lock(this->_mutex);
            this->do_add(app);
        }

        void remove(application_id_type id);

        void loop() override;
        void forward(sbn::kernel_ptr&& hdr) override;

        inline void pipe_buffer_size(size_t rhs) noexcept { this->_pipe_buffer_size = rhs; }
        inline void allow_root(bool rhs) noexcept { this->_allowroot = rhs; }
        inline void interleave(bool rhs) noexcept { this->_interleave = rhs; }
        inline void timeout(duration rhs) noexcept { this->_timeout = rhs; }
        inline void kernel_timeout(duration rhs) noexcept { this->_kernel_timeout = rhs; }

        inline const application_table& jobs() const noexcept {
            return this->_jobs;
        }

        inline sentry guard() noexcept { return sentry(*this); }
        inline pipeline* unix() const noexcept { return this->_unix; }
        inline void unix(pipeline* rhs) noexcept { this->_unix = rhs; }
        inline void max_threads(unsigned rhs) noexcept { this->_max_threads = rhs; }

        void clear(sbn::kernel_sack& sack);

    protected:

        void process_kernels() override;
        void process_connections() override;

    private:

        app_iterator do_add(const sbn::application& app);
        void do_forward(sbn::kernel_ptr&& k);
        void process_kernel(sbn::kernel_ptr&& k);
        void wait_loop();
        void wait_for_processes(lock_type& lock);
        void terminate(sys::pid_type id);

        app_iterator find_by_process_id(sys::pid_type pid);

        friend class process_handler;

        inline sbn::weight_array total_load() const noexcept {
            sbn::weight_array sum;
            for (auto& pair : this->_jobs) {
                sum += pair.second->load();
            }
            return sum;
        }

        inline sbn::weight_type total_threads_used() const noexcept {
            const auto& load = total_load();
            return load[0]*this->_max_threads + load[1];
        }

        inline sbn::weight_array::value_type
        num_threads_used(const sbn::weight_array& w) const noexcept {
            return w[0]*this->_max_threads + w[1];
        }

        void do_process_kernels(kernel_queue& kernels, sbn::weight_array& current_load);

    };

}

#endif // vim:filetype=cpp
