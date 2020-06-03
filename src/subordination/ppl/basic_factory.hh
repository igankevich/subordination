#ifndef SUBORDINATION_PPL_BASIC_FACTORY_HH
#define SUBORDINATION_PPL_BASIC_FACTORY_HH

#include <iosfwd>

#include <subordination/ppl/application.hh>
#include <subordination/ppl/basic_pipeline.hh>
#include <subordination/ppl/child_process_pipeline.hh>
#include <subordination/ppl/parallel_pipeline.hh>

namespace sbn {

    class Factory: public pipeline_base {

    private:
        parallel_pipeline _local;
        child_process_pipeline _remote;

    public:

        Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void send(kernel* k) { this->_local.send(k); }
        inline void send_remote(kernel* k) { this->_remote.send(k); }
        inline void schedule(kernel* k) { this->_local.send_timer(k); }
        inline void schedule(kernel** k, size_t n) { this->_local.send(k, n); }

        inline child_process_pipeline& parent() noexcept { return this->_remote; }
        inline const child_process_pipeline& parent() const noexcept { return this->_remote; }

        void start();
        void stop();
        void wait();
        void clear();

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
