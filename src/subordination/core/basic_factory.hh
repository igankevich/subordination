#ifndef SUBORDINATION_CORE_BASIC_FACTORY_HH
#define SUBORDINATION_CORE_BASIC_FACTORY_HH

#include <iosfwd>

#include <subordination/core/application.hh>
#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/child_process_pipeline.hh>
#include <subordination/core/parallel_pipeline.hh>

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
