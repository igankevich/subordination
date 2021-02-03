#ifndef SUBORDINATION_DAEMON_DISCOVERER_HH
#define SUBORDINATION_DAEMON_DISCOVERER_HH

#include <chrono>
#include <iosfwd>

#include <unistdx/base/log_message>
#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

#include <subordination/core/properties.hh>
#include <subordination/daemon/config.hh>
#include <subordination/daemon/hierarchy.hh>
#include <subordination/daemon/hierarchy_kernel.hh>
#include <subordination/daemon/probe.hh>
#include <subordination/daemon/resident_kernel.hh>
#include <subordination/daemon/socket_pipeline_event.hh>
#include <subordination/daemon/tree_hierarchy_iterator.hh>
#include <subordination/daemon/types.hh>


namespace sbnd {

    enum class probe_result {
        add_subordinate = 0,
        remove_subordinate,
        reject_subordinate,
        retain
    };

    std::ostream&
    operator<<(std::ostream& out, probe_result rhs);

    class discoverer: public resident_kernel {

    public:
        struct properties {
            sbn::Duration scan_interval = std::chrono::minutes(1);
            sys::path cache_directory;
            sys::ipv4_address::rep_type fanout = 64;
            int max_attempts = 1;
            int max_radius = 100;
            bool profile = false;
            bool set(const char* key, const std::string& value);
        };

    public:
        using addr_type = sys::ipv4_address;
        using uint_type = addr_type::rep_type;
        using ifaddr_type = sys::interface_address<addr_type>;
        using iterator = tree_hierarchy_iterator<addr_type>;
        using hierarchy_type = Hierarchy<addr_type>;
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using resource_array = sbn::resource_array;
        using hierarchy_node_array = std::vector<hierarchy_node>;

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
        int _max_radius = 100;
        bool _profile = false;

    public:

        discoverer(const ifaddr_type& interface_address,
                   const sys::port_type port,
                   const properties& p);

        void on_start() override;
        void on_kernel(sbn::kernel_ptr&& k) override;

        inline const hierarchy_type& hierarchy() const noexcept { return this->_hierarchy; }

        void read_cache();

        void resources(const sbn::resource_array& rhs, hierarchy_node::time_point now);

    private:

        inline duration interval() const noexcept { return this->_interval; }
        inline bool profile() const noexcept { return this->_profile; }
        inline int max_attempts() const noexcept { return this->_max_attempts; }
        inline const sys::path& cache_directory() const noexcept { return this->_cache_directory; }

        inline ifaddr_type interface_address() const noexcept {
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
        void probe_received(pointer<probe> p);
        probe_result process_probe(pointer<probe>& p);
        void probe_returned(pointer<probe> p);
        //void send_probe(const sys::socket_address& dest);

        inline states state() const noexcept { return this->_state; }
        inline void state(states rhs) noexcept { this->_state = rhs; }

        void on_event(pointer<socket_pipeline_kernel> k);
        void on_client_add(const sys::socket_address& endp);
        void on_client_remove(const sys::socket_address& endp);
        void broadcast_hierarchy(const sys::socket_address& ignored_endpoint);
        std::string cache_filename() const;
        void write_cache() const;
        void send_weight(const sys::socket_address& dest,
                         const hierarchy_node_array& nodes);
        void update_weights(pointer<Hierarchy_kernel> k);
        void update_socket_pipeline_clients();

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
