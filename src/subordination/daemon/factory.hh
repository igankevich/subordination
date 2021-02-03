#ifndef SUBORDINATION_DAEMON_FACTORY_HH
#define SUBORDINATION_DAEMON_FACTORY_HH

#include <subordination/bits/contracts.hh>
#include <subordination/core/factory_properties.hh>
#include <subordination/core/kernel_instance_registry.hh>
#include <subordination/core/kernel_type_registry.hh>
#include <subordination/core/parallel_pipeline.hh>
#include <subordination/core/properties.hh>
#include <subordination/core/transaction_log.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/discoverer.hh>
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

    auto string_to_factory_flags(const std::string& s) -> factory_flags;
    std::istream& operator>>(std::istream& in, sbnd::factory_flags& rhs);

    class interface_address_list:
        public std::vector<sys::interface_address<sys::ipv4_address>> {
    public:
        using std::vector<sys::interface_address<sys::ipv4_address>>::vector;
    };

    auto string_to_interface_address_list(const std::string& s) -> interface_address_list;

    class Properties: public sbn::properties {

    public:
        sbn::parallel_pipeline::properties local;
        socket_pipeline::properties remote;
        process_pipeline::properties process;
        unix_socket_pipeline::properties unix;
        struct { factory_flags flags = factory_flags::all; } factory;
        sbn::transaction_log::properties transactions;
        struct {
            interface_address_list allowed_interface_addresses;
            sbn::Duration interface_update_interval = std::chrono::minutes(1);
        } network;
        discoverer::properties discover;
        struct Resources {
            std::unordered_map<std::string,sbn::resources::expression_ptr> expressions;
        } resources;
        #if defined(SBND_WITH_GLUSTERFS)
        struct GlusterFS {
            sys::path working_directory{"/var/lib/glusterd"};
        } glusterfs;
        #endif

    public:
        Properties();
        void property(const std::string& key, const std::string& value) override;

    };

    /// A container with one or nought elements.
    template <class T>
    class storage {
    private:
        typename std::aligned_storage<sizeof(T),alignof(T)>::type _data;
        bool _set = false;
    public:
        template <class ... Args>
        inline storage(Args&& ... args) {
            new (&this->_data) T{std::forward<Args>(args)...};
            this->_set = true;
        }
        inline ~storage() noexcept { destroy(); }
        template <class ... Args>
        inline void make(Args&& ... args) {
            destroy();
            this->_set = false;
            new (&this->_data) T{std::forward<Args>(args)...};
            this->_set = true;
        }
        inline void destroy() noexcept { if (*this) { get()->~T(); } }
        inline T* get() noexcept {
            return reinterpret_cast<T*>(&this->_data);
        }
        inline const T* get() const noexcept {
            return reinterpret_cast<const T*>(&this->_data);
        }
        // smart pointer interface
        inline T* operator&() noexcept { return get(); }
        inline const T* operator&() const noexcept { return get(); }
        inline T& operator*() noexcept {
            Expects(this->_set);
            return *get();
        }
        inline const T& operator*() const noexcept {
            Expects(this->_set);
            return *get();
        }
        inline T* operator->() noexcept { return get(); }
        inline const T* operator->() const noexcept { return get(); }
        inline explicit operator bool() const noexcept { return this->_set; }
        inline bool operator!() const noexcept { return !this->_set; }
        // container interface
        inline T* data() noexcept { return get(); }
        inline const T* data() const noexcept { return get(); }
        inline size_t size() const noexcept { return static_cast<size_t>(this->_set); }
        inline T* begin() noexcept { return get(); }
        inline const T* begin() const noexcept { return get(); }
        inline T* end() noexcept { return get() + size(); }
        inline const T* end() const noexcept { return get() + size(); }
        storage() = default;
        storage(const storage&) = delete;
        storage& operator=(const storage&) = delete;
        storage(storage&&) = delete;
        storage& operator=(storage&&) = delete;
    };

    class Factory {

    private:
        storage<sbn::parallel_pipeline> _local;
        storage<socket_pipeline> _remote;
        storage<process_pipeline> _process;
        storage<unix_socket_pipeline> _unix;
        sbn::kernel_type_registry _types;
        sbn::kernel_instance_registry _instances;
        storage<sbn::transaction_log> _transactions;
        factory_flags _flags = factory_flags::all;

    public:

        inline void send(sbn::kernel_ptr&& k) { this->_local->send(std::move(k)); }
        inline void send_remote(sbn::kernel_ptr&& k) { this->_remote->send(std::move(k)); }
        inline void send_unix(sbn::kernel_ptr&& k) { this->_unix->send(std::move(k)); }
        inline void send_child(sbn::kernel_ptr&& k) { this->_process->send(std::move(k)); }
        inline void schedule(sbn::kernel_ptr&& k) { this->_local->send(std::move(k)); }
        inline void schedule(sbn::kernel_ptr_array&& k) { this->_local->send(std::move(k)); }
        inline sbn::kernel_type_registry& types() noexcept { return this->_types; }
        inline sbn::kernel_instance_registry& instances() noexcept { return this->_instances; }

        inline sbn::parallel_pipeline& local() noexcept { return *this->_local; }
        inline socket_pipeline& remote() noexcept { return *this->_remote; }
        inline unix_socket_pipeline& unix() noexcept { return *this->_unix; }
        inline process_pipeline& process() noexcept { return *this->_process; }
        inline const process_pipeline& process() const noexcept { return *this->_process; }

        void transactions(const char* filename);
        inline sbn::transaction_log& transactions() noexcept { return *this->_transactions; }

        inline factory_flags flags() const noexcept { return this->_flags; }
        inline void flags(factory_flags rhs) noexcept { this->_flags = rhs; }
        inline void setf(factory_flags f) noexcept { this->_flags |= f; }
        inline void unsetf(factory_flags f) noexcept { this->_flags &= ~f; }
        inline bool isset(factory_flags f) const noexcept { return bool(this->_flags & f); }

        void start();
        void stop();
        void wait();
        void clear();
        void configure(const Properties& props);

        Factory() = default;
        ~Factory() = default;
        Factory(const Factory&) = delete;
        Factory& operator=(const Factory&) = delete;
        Factory(Factory&&) = delete;
        Factory& operator=(Factory&&) = delete;

    };

    extern Factory factory;

}

#endif // vim:filetype=cpp
