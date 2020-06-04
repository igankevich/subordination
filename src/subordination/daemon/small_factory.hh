#ifndef SUBORDINATION_DAEMON_SMALL_FACTORY_HH
#define SUBORDINATION_DAEMON_SMALL_FACTORY_HH

#include <subordination/core/parallel_pipeline.hh>
#include <subordination/core/socket_pipeline.hh>

namespace sbnc {


    class Factory {

    private:
        // we do not need downstream pipeline here since
        // there is only one thread
        sbn::parallel_pipeline _local{1};
        sbn::socket_pipeline _remote;

    public:
        Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory& operator=(const Factory&) = delete;
        Factory(Factory&&) = delete;
        Factory& operator=(Factory&&) = delete;

        inline sbn::parallel_pipeline& local() noexcept { return this->_local; }
        inline sbn::socket_pipeline& remote() noexcept { return this->_remote; }

        void start();
        void stop();
        void wait();

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
