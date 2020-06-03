#ifndef SUBORDINATION_PPL_BASIC_FACTORY_HH
#define SUBORDINATION_PPL_BASIC_FACTORY_HH

#include <iosfwd>

#include <subordination/ppl/application.hh>
#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/child_process_pipeline.hh>
#include <subordination/ppl/io_pipeline.hh>
#include <subordination/ppl/multi_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>
#include <subordination/ppl/process_pipeline.hh>
#include <subordination/ppl/timer_pipeline.hh>

namespace sbn {

    class Factory: public pipeline_base {

    private:
        parallel_pipeline _native;
        multi_pipeline _downstream;
        timer_pipeline _scheduled;
        #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
        io_pipeline _io;
        #endif
        child_process_pipeline _parent;

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

        inline void schedule(kernel* k) { this->_scheduled.send(k); }
        inline void schedule(kernel** k, size_t n) { this->_scheduled.send(k, n); }

        inline child_process_pipeline& parent() noexcept { return this->_parent; }
        inline const child_process_pipeline& parent() const noexcept { return this->_parent; }

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

    extern Factory factory;

}

#endif // vim:filetype=cpp
