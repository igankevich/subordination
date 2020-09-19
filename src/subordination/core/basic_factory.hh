#ifndef SUBORDINATION_CORE_BASIC_FACTORY_HH
#define SUBORDINATION_CORE_BASIC_FACTORY_HH

#include <iosfwd>

#include <subordination/core/application.hh>
#include <subordination/core/basic_pipeline.hh>
#include <subordination/core/child_process_pipeline.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>

namespace sbn {

    class Factory {

    private:
        parallel_pipeline _local;
        child_process_pipeline _remote;
        kernel_type_registry _types;
        kernel_instance_registry _instances;

    public:

        Factory();
        virtual ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void send(kernel_ptr&& k) { this->_local.send(std::move(k)); }
        inline void send_remote(kernel_ptr&& k) { this->_remote.send(std::move(k)); }
        inline void schedule(kernel_ptr&& k) { this->_local.send_timer(std::move(k)); }
        inline void schedule(kernel_ptr_array&& k) { this->_local.send(std::move(k)); }

        inline parallel_pipeline& local() noexcept { return this->_local; }
        inline const parallel_pipeline& local() const noexcept { return this->_local; }
        inline child_process_pipeline& remote() noexcept { return this->_remote; }
        inline const child_process_pipeline& remote() const noexcept { return this->_remote; }
        inline sbn::kernel_type_registry& types() noexcept { return this->_types; }
        inline sbn::kernel_instance_registry& instances() noexcept { return this->_instances; }

        void start();
        void stop();
        void wait();
        void clear();

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
