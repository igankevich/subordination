#ifndef SUBORDINATION_DAEMON_FACTORY_HH
#define SUBORDINATION_DAEMON_FACTORY_HH

#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/core/transaction_log.hh>
#include <subordination/daemon/process_pipeline.hh>
#include <subordination/daemon/socket_pipeline.hh>
#include <subordination/daemon/unix_socket_pipeline.hh>

namespace sbnd {

    class Factory: public sbn::pipeline_base {

    private:
        sbn::parallel_pipeline _local;
        socket_pipeline _remote;
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        process_pipeline _process;
        unix_socket_pipeline _unix;
        #endif
        sbn::kernel_type_registry _types;
        sbn::kernel_instance_registry _instances;
        sbn::transaction_log _transactions;

    public:

        explicit Factory(unsigned concurrency);
        explicit Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void send(sbn::kernel* k) { this->_local.send(k); }
        inline void send_remote(sbn::kernel* k) { this->_remote.send(k); }

        inline void
        send_unix(sbn::kernel* k) {
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_unix.send(k);
            #endif
        }

        inline void schedule(sbn::kernel* k) { this->_local.send(k); }
        inline void schedule(sbn::kernel** k, size_t n) { this->_local.send(k, n); }
        inline sbn::kernel_type_registry& types() noexcept { return this->_types; }
        inline sbn::kernel_instance_registry& instances() noexcept { return this->_instances; }

        inline sbn::parallel_pipeline& local() noexcept { return this->_local; }
        inline socket_pipeline& remote() noexcept { return this->_remote; }
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        inline unix_socket_pipeline& unix() noexcept { return this->_unix; }
        #endif

        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        inline void
        send_child(sbn::kernel* k) {
            this->_process.send(k);
        }

        inline process_pipeline& process() noexcept { return this->_process; }
        inline const process_pipeline& process() const noexcept { return this->_process; }

        #endif

        inline void start() {
            this->_local.start();
            this->_remote.start();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.start();
            this->_unix.start();
            #endif
        }

        inline void stop() {
            this->_local.stop();
            this->_remote.stop();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.stop();
            this->_unix.stop();
            #endif
        }

        inline void wait() {
            this->_local.wait();
            this->_remote.wait();
            #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_process.wait();
            this->_unix.wait();
            #endif
        }

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
