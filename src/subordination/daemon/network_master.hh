#ifndef SUBORDINATION_DAEMON_NETWORK_MASTER_HH
#define SUBORDINATION_DAEMON_NETWORK_MASTER_HH

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <unistdx/net/interface_addresses>

#include <subordination/daemon/config.hh>
#include <subordination/daemon/master_discoverer.hh>
#include <subordination/daemon/socket_pipeline_event.hh>
#include <subordination/daemon/types.hh>

namespace sbnd {

    class network_timer: public sbn::kernel {};

    class network_master: public sbn::kernel {

    private:
        typedef sys::ipv4_address addr_type;
        typedef addr_type::rep_type uint_type;
        typedef sys::interface_address<addr_type> ifaddr_type;
        typedef typename sys::ipaddr_traits<addr_type> traits_type;
        typedef std::unordered_set<ifaddr_type> interface_address_set;
        typedef std::unordered_map<ifaddr_type,master_discoverer*> discoverer_table;
        typedef typename discoverer_table::iterator map_iterator;

    private:
        Properties::Discoverer _discoverer_properties;
        discoverer_table _discoverers;
        interface_address_set _allowedifaddrs;
        /// Interface address list update interval.
        duration _interval = std::chrono::minutes(1);

    public:

        network_master() = default;
        network_master(const Properties& props);

        void act() override;
        void react(sbn::kernel_ptr&& child) override;
        void mark_as_deleted(sbn::kernel_sack& result) override;

    private:

        void send_timer(bool first_time=false);

        interface_address_set
        enumerate_ifaddrs();

        void
        update_ifaddrs();

        void
        add_ifaddr(const ifaddr_type& rhs);

        void
        remove_ifaddr(const ifaddr_type& rhs);

        /// forward the probe to an appropriate discoverer
        void forward_probe(pointer<probe> p);
        void forward_hierarchy_kernel(pointer<Hierarchy_kernel> p);
        void on_event(pointer<socket_pipeline_kernel> k);
        void on_event(pointer<process_pipeline_kernel> k);
        void report_status(pointer<Status_kernel> k);
        void report_job_status(pointer<Job_status_kernel> k);
        void report_pipeline_status(pointer<Pipeline_status_kernel> k);

        map_iterator find_discoverer(const addr_type& a);

        inline bool
        is_allowed(const ifaddr_type& rhs) const {
            for (const ifaddr_type& ifa : this->_allowedifaddrs) {
                if (ifa.contains(rhs.address())) {
                    return true;
                }
            }
            return false;
        }

        template <class ... Args> inline void
        log(const char* fmt, const Args& ... args) const {
            sys::log_message("net", fmt, args ...);
        }

    };

}

#endif // vim:filetype=cpp
