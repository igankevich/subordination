#ifndef SUBORDINATION_DAEMON_SMALL_FACTORY_HH
#define SUBORDINATION_DAEMON_SMALL_FACTORY_HH

#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/daemon/socket_pipeline.hh>

namespace sbnc {


    class Factory {

    private:
        // we do not need downstream pipeline here since
        // there is only one thread
        sbn::parallel_pipeline _local{1};
        sbnd::socket_pipeline _remote;
        sbn::kernel_type_registry _types;

    public:
        Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory& operator=(const Factory&) = delete;
        Factory(Factory&&) = delete;
        Factory& operator=(Factory&&) = delete;

        inline sbn::parallel_pipeline& local() noexcept { return this->_local; }
        inline sbnd::socket_pipeline& remote() noexcept { return this->_remote; }
        inline sbn::kernel_type_registry& types() noexcept { return this->_types; }

        void start();
        void stop();
        void wait();
        void clear();

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
