#ifndef SUBORDINATION_DAEMON_MASTER_DISCOVERER_HH
#define SUBORDINATION_DAEMON_MASTER_DISCOVERER_HH

#include <chrono>
#include <iosfwd>

#include <unistdx/base/log_message>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/daemon/config.hh>
#include <subordination/daemon/hierarchy.hh>
#include <subordination/daemon/hierarchy_kernel.hh>
#include <subordination/daemon/probe.hh>
#include <subordination/daemon/prober.hh>
#include <subordination/daemon/resident_kernel.hh>
#include <subordination/daemon/socket_pipeline_event.hh>
#include <subordination/daemon/tree_hierarchy_iterator.hh>
#include <subordination/daemon/types.hh>


namespace sbnd {

    /// Timer which is used to periodically scan nodes
    /// to find the best principal node.
    class discovery_timer: public sbn::kernel {};

    enum class probe_result {
        add_subordinate = 0,
        remove_subordinate,
        reject_subordinate,
        retain
    };

    std::ostream&
    operator<<(std::ostream& out, probe_result rhs);

    class master_discoverer: public resident_kernel {

    public:
        using addr_type = sys::ipv4_address;
        using uint_type = addr_type::rep_type;
        using ifaddr_type = sys::interface_address<addr_type>;
        using iterator = tree_hierarchy_iterator<addr_type>;
        using hierarchy_type = Hierarchy<addr_type>;
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using weight_type = typename hierarchy_type::weight_type;

        enum class states {
            initial,
            waiting,
            probing,
        };

    private:
        /// Time period between subsequent network scans.
        duration _interval = std::chrono::minutes(1);
        uint_type _fanout = 10000;
        hierarchy_type _hierarchy;
        iterator _iterator, _end;
        sys::path _cache_directory{SBND_SHARED_STATE_DIR};
        states _state = states::initial;
        int _attempts = 0;
        int _max_attempts = 3;
        bool _profile = false;

    public:
        inline
        master_discoverer(
            const ifaddr_type& interface_address,
            const sys::port_type port,
            uint_type fanout
        ): _fanout(fanout), _hierarchy(interface_address, port)
        { reset_iterator(); }

        void on_start() override;
        void on_kernel(sbn::kernel_ptr&& k) override;

        inline const hierarchy_type& hierarchy() const noexcept { return this->_hierarchy; }
        inline void interval(duration rhs) noexcept { this->_interval = rhs; }
        inline duration interval() const noexcept { return this->_interval; }
        inline bool profile() const noexcept { return this->_profile; }
        inline void profile(bool rhs) noexcept { this->_profile = rhs; }
        inline int max_attempts() const noexcept { return this->_max_attempts; }
        inline void max_attempts(int rhs) noexcept { this->_max_attempts = rhs; }
        inline const sys::path& cache_directory() const noexcept { return this->_cache_directory; }
        inline void cache_directory(const sys::path& rhs) noexcept { this->_cache_directory = rhs; }

        void read_cache();

    private:

        const ifaddr_type&
        interface_address() const noexcept {
            return this->_hierarchy.interface_address();
        }

        sys::port_type
        port() const noexcept {
            return this->_hierarchy.port();
        }

        void on_timer();
        void discover();
        void discover_later();
        void reset_iterator();
        void update_subordinates(pointer<probe> p);
        void add_subordinate(const sys::socket_address& address);
        void add_superior(const sys::socket_address& address, weight_type weight);
        void remove_subordinate(const sys::socket_address& address);
        void remove_superior();
        probe_result process_probe(pointer<probe>& p);
        void update_superior(pointer<prober> p);

        inline states state() const noexcept { return this->_state; }
        inline void state(states rhs) noexcept { this->_state = rhs; }

        void on_event(pointer<socket_pipeline_kernel> k);
        void on_client_add(const sys::socket_address& endp);
        void on_client_remove(const sys::socket_address& endp);
        void broadcast_hierarchy(sys::socket_address ignored_endpoint = sys::socket_address());
        std::string cache_filename() const;
        void write_cache() const;
        void send_weight(const sys::socket_address& dest, weight_type w);
        void update_weights(pointer<Hierarchy_kernel> k);

        template <class ... Args> inline void
        log(const char* fmt, const Args& ... args) const {
            sys::log_message("discoverer", fmt, args ...);
        }

        template <class ... Args> inline void
        profile(const char* fmt, const Args& ... args) const {
            sys::log_message("profile-node-discovery", fmt, args...);
        }

    };


}

#endif // vim:filetype=cpp
