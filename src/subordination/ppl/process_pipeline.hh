#ifndef SUBORDINATION_PPL_PROCESS_PIPELINE_HH
#define SUBORDINATION_PPL_PROCESS_PIPELINE_HH

#include <memory>
#include <unordered_map>

#include <unistdx/ipc/process>
#include <unistdx/ipc/process_group>
#include <unistdx/net/pstream>

#include <subordination/kernel/kernel_header.hh>
#include <subordination/ppl/application.hh>
#include <subordination/ppl/basic_socket_pipeline.hh>
#include <subordination/ppl/process_handler.hh>

namespace sbn {

    template<class K, class R>
    class process_pipeline: public basic_socket_pipeline<K> {

    private:
        typedef basic_socket_pipeline<K> base_pipeline;
        using typename base_pipeline::queue_popper;
        using typename base_pipeline::lock_type;
        using typename base_pipeline::mutex_type;
        typedef process_handler<K,R> event_handler_type;
        typedef std::shared_ptr<event_handler_type> event_handler_ptr;
        typedef std::unordered_map<application_type,event_handler_ptr> map_type;
        typedef typename map_type::iterator app_iterator;

    public:
        typedef R router_type;
        typedef K kernel_type;

    private:
        map_type _apps;
        sys::process_group _procs;
        /// Allow process execution as superuser/supergroup.
        bool _allowroot = false;

    public:

        process_pipeline() = default;

        process_pipeline(const process_pipeline&) = delete;

        process_pipeline&
        operator=(const process_pipeline&) = delete;

        process_pipeline(process_pipeline&& rhs) = default;

        ~process_pipeline() = default;

        inline void
        add(const application& app) {
            lock_type lock(this->_mutex);
            this->do_add(app);
        }

        void
        do_run() override;

        void
        forward(foreign_kernel* hdr);

        inline void
        allow_root(bool rhs) noexcept {
            this->_allowroot = rhs;
        }

        void
        print_state(std::ostream& out);

    protected:

        void
        process_kernels() override;

    private:

        app_iterator
        do_add(const application& app);

        void
        process_kernel(kernel_type* k);

        void
        wait_for_all_processes_to_finish();

        void
        on_process_exit(const sys::process& p, sys::process_status status);

        inline app_iterator
        find_by_app_id(application_type id) {
            return this->_apps.find(id);
        }

        app_iterator
        find_by_process_id(sys::pid_type pid);

        template <class X, class Y> friend class process_notify_handler;
        template <class X, class Y> friend class process_handler;

    };

}

#endif // vim:filetype=cpp
