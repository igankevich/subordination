#ifndef SUBORDINATION_DAEMON_FACTORY_HH
#define SUBORDINATION_DAEMON_FACTORY_HH

#include <subordination/daemon/unix_socket_pipeline.hh>
#include <subordination/ppl/multi_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#include <subordination/ppl/process_pipeline.hh>
#include <subordination/ppl/socket_pipeline.hh>
#include <subordination/ppl/timer_pipeline.hh>

namespace sbnd {

    class Factory: public sbn::pipeline_base {

    private:
        sbn::parallel_pipeline _local;
        sbn::multi_pipeline _downstream;
        sbn::timer_pipeline _scheduled;
        sbn::socket_pipeline _remote;
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        sbn::process_pipeline _process;
        unix_socket_pipeline _external;
        #endif

    public:

        explicit Factory(unsigned concurrency);
        explicit Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void
        send(sbn::kernel* k) {
            if (k->scheduled()) {
                this->_scheduled.send(k);
            } else if (k->moves_downstream()) {
                const size_t i = k->hash();
                const size_t n = this->_downstream.size();
                this->_downstream[i%n].send(k);
            } else {
                this->_local.send(k);
            }
        }

        inline void
        send_remote(sbn::kernel* k) {
            this->_remote.send(k);
        }

        inline void
        send_external(sbn::kernel* k) {
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_external.send(k);
            #endif
        }

        inline void schedule(sbn::kernel* k) { this->_scheduled.send(k); }
        inline void schedule(sbn::kernel** k, size_t n) { this->_scheduled.send(k, n); }

        inline sbn::parallel_pipeline& local() noexcept { return this->_local; }
        inline sbn::socket_pipeline& remote() noexcept { return this->_remote; }
        inline sbn::timer_pipeline& timer() noexcept { return this->_scheduled; }
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        inline unix_socket_pipeline& external() noexcept { return this->_external; }
        #endif

        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        inline void
        send_child(sbn::kernel* k) {
            this->_process.send(k);
        }

        inline sbn::process_pipeline&
        child() noexcept {
            return this->_process;
        }

        inline const sbn::process_pipeline&
        child() const noexcept {
            return this->_process;
        }

        #endif

        inline void start() {
            this->_local.start();
            this->_scheduled.start();
            this->_remote.start();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.start();
            this->_external.start();
            #endif
        }

        inline void stop() {
            this->_local.stop();
            this->_scheduled.stop();
            this->_remote.stop();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.stop();
            this->_external.stop();
            #endif
        }

        inline void wait() {
            this->_local.wait();
            this->_scheduled.wait();
            this->_remote.wait();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.wait();
            this->_external.wait();
            #endif
        }

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
