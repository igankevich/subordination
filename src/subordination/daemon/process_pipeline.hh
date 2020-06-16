#ifndef SUBORDINATION_DAEMON_PROCESS_PIPELINE_HH
#define SUBORDINATION_DAEMON_PROCESS_PIPELINE_HH

#include <memory>
#include <unordered_map>

#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>

#include <subordination/core/application.hh>
#include <subordination/core/basic_socket_pipeline.hh>
#include <subordination/core/process_handler.hh>

namespace sbnd {

    class process_pipeline: public sbn::basic_socket_pipeline {

    private:
        using connection_type = sbn::process_handler;
        using connection_ptr = std::shared_ptr<connection_type>;
        using application_id_type = sbn::application::id_type;
        using application_table = std::unordered_map<application_id_type,connection_ptr>;
        using app_iterator = typename application_table::iterator;

    private:
        application_table _jobs;
        sys::process_group _child_processes;
        /// How long a child process lives without receiving/sending kernels.
        duration _timeout;
        /// Allow process execution as superuser/supergroup.
        bool _allowroot = true;

    public:

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
        void forward(sbn::foreign_kernel* hdr) override;

        inline void allow_root(bool rhs) noexcept { this->_allowroot = rhs; }
        inline void timeout(duration rhs) noexcept { this->_timeout = rhs; }

        inline const application_table& jobs() const noexcept {
            return this->_jobs;
        }

        inline sentry guard() noexcept { return sentry(*this); }

    protected:

        void process_kernels() override;
        void process_connections() override;

    private:

        app_iterator do_add(const sbn::application& app);
        void process_kernel(sbn::kernel* k);
        void wait_loop();
        void wait_for_processes(lock_type& lock);
        void terminate(sys::pid_type id);

        app_iterator find_by_process_id(sys::pid_type pid);

        friend class process_handler;

    };

}

#endif // vim:filetype=cpp
