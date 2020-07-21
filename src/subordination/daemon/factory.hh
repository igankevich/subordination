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

    enum class factory_flags: sys::u32 {
        local = 1<<0,
        remote = 1<<1,
        process = 1<<2,
        unix = 1<<3,
        transactions = 1<<4,
        all = std::numeric_limits<sys::u32>::max(),
    };

    UNISTDX_FLAGS(factory_flags);

    class Factory: public sbn::pipeline_base {

    private:
        sbn::parallel_pipeline _local;
        socket_pipeline _remote;
        process_pipeline _process;
        unix_socket_pipeline _unix;
        sbn::kernel_type_registry _types;
        sbn::kernel_instance_registry _instances;
        sbn::transaction_log _transactions;
        factory_flags _flags = factory_flags::all;

    public:

        explicit Factory(unsigned concurrency);
        explicit Factory();
        ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory(Factory&&) = delete;

        inline void send(sbn::kernel_ptr&& k) { this->_local.send(std::move(k)); }
        inline void send_remote(sbn::kernel_ptr&& k) { this->_remote.send(std::move(k)); }
        inline void send_unix(sbn::kernel_ptr&& k) { this->_unix.send(std::move(k)); }
        inline void send_child(sbn::kernel_ptr&& k) { this->_process.send(std::move(k)); }
        inline void schedule(sbn::kernel_ptr&& k) { this->_local.send(std::move(k)); }
        inline void schedule(sbn::kernel_ptr_array&& k) { this->_local.send(std::move(k)); }
        inline sbn::kernel_type_registry& types() noexcept { return this->_types; }
        inline sbn::kernel_instance_registry& instances() noexcept { return this->_instances; }

        inline sbn::parallel_pipeline& local() noexcept { return this->_local; }
        inline socket_pipeline& remote() noexcept { return this->_remote; }
        inline unix_socket_pipeline& unix() noexcept { return this->_unix; }
        inline process_pipeline& process() noexcept { return this->_process; }
        inline const process_pipeline& process() const noexcept { return this->_process; }

        void transactions(const char* filename);
        inline sbn::transaction_log& transactions() noexcept { return this->_transactions; }

        inline factory_flags flags() const noexcept { return this->_flags; }
        inline void flags(factory_flags rhs) noexcept { this->_flags = rhs; }
        inline void setf(factory_flags f) noexcept { this->_flags |= f; }
        inline void unsetf(factory_flags f) noexcept { this->_flags &= ~f; }
        inline bool isset(factory_flags f) const noexcept { return bool(this->_flags & f); }

        void start();
        void stop();
        void wait();
        void clear();

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
