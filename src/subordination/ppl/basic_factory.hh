#ifndef SUBORDINATION_PPL_BASIC_FACTORY_HH
#define SUBORDINATION_PPL_BASIC_FACTORY_HH

#if !defined(SUBORDINATION_DAEMON) && \
    !defined(SUBORDINATION_APPLICATION) && \
    !defined(SUBORDINATION_SUBMIT)
#define SUBORDINATION_APPLICATION
#endif

#include <iosfwd>

#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/io_pipeline.hh>
#include <subordination/ppl/multi_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#if defined(SUBORDINATION_DAEMON) || defined(SUBORDINATION_SUBMIT)
#include <subordination/ppl/socket_pipeline.hh>
#include <unistdx/net/socket>
#endif
#include <subordination/ppl/process_pipeline.hh>
#if defined(SUBORDINATION_APPLICATION)
#include <subordination/ppl/child_process_pipeline.hh>
#endif
#if defined(SUBORDINATION_DAEMON)
#include <subordination/ppl/unix_socket_pipeline.hh>
#endif
#include <subordination/ppl/application.hh>
#include <subordination/ppl/timer_pipeline.hh>

namespace sbn {

    class Factory: public pipeline_base {

    public:
        #if defined(SUBORDINATION_APPLICATION)
        using parent_pipeline_type = child_process_pipeline;
        #elif defined(SUBORDINATION_DAEMON) || defined(SUBORDINATION_SUBMIT)
        using parent_pipeline_type = socket_pipeline;
        #endif

    private:
        parallel_pipeline _native;
        multi_pipeline _downstream;
        timer_pipeline _scheduled;
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        io_pipeline _io;
        #endif
        parent_pipeline_type _parent;
        #if defined(SUBORDINATION_DAEMON) && \
        !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        process_pipeline _child;
        unix_socket_pipeline _external;
        #endif

    public:

        Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void
        send(kernel* k) {
            if (k->scheduled()) {
                this->_scheduled.send(k);
            } else if (k->moves_downstream()) {
                const size_t i = k->hash();
                const size_t n = this->_downstream.size();
                this->_downstream[i%n].send(k);
            } else {
                this->_native.send(k);
            }
        }

        inline void
        send_remote(kernel* k) {
            this->_parent.send(k);
        }

        inline void
        send_external(kernel* k) {
            #if defined(SUBORDINATION_DAEMON) && \
            !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
            this->_external.send(k);
            #endif
        }

        inline void schedule(kernel* k) { this->_scheduled.send(k); }
        inline void schedule(kernel** k, size_t n) { this->_scheduled.send(k, n); }

        #if defined(SUBORDINATION_DAEMON) && \
        !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        inline void
        send_child(kernel* k) {
            this->_child.send(k);
        }

        inline process_pipeline&
        child() noexcept {
            return this->_child;
        }

        inline const process_pipeline&
        child() const noexcept {
            return this->_child;
        }

        inline unix_socket_pipeline&
        external() noexcept {
            return this->_external;
        }

        inline const unix_socket_pipeline&
        external() const noexcept {
            return this->_external;
        }

        #endif

        inline parent_pipeline_type&
        parent() noexcept {
            return this->_parent;
        }

        inline const parent_pipeline_type&
        parent() const noexcept {
            return this->_parent;
        }

        #if defined(SUBORDINATION_DAEMON)
        inline parent_pipeline_type&
        nic() noexcept {
            return this->_parent;
        }

        inline const parent_pipeline_type&
        nic() const noexcept {
            return this->_parent;
        }

        #endif

        inline timer_pipeline&
        timer() noexcept {
            return this->_scheduled;
        }

        inline const timer_pipeline&
        timer() const noexcept {
            return this->_scheduled;
        }

        void start();
        void stop();
        void wait();

    };

    using factory_type = Factory;

    extern factory_type factory;

}

#endif // vim:filetype=cpp
