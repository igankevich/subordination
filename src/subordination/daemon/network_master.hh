#ifndef SUBORDINATION_DAEMON_NETWORK_MASTER_HH
#define SUBORDINATION_DAEMON_NETWORK_MASTER_HH

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <unistdx/net/interface_addresses>

#include <subordination/daemon/master_discoverer.hh>
#include <subordination/ppl/socket_pipeline_event.hh>

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
        discoverer_table _discoverers;
        interface_address_set _allowedifaddrs;
        uint_type _fanout = 10000;
        network_timer* _timer = nullptr;
        /// Interface address list update interval.
        std::chrono::milliseconds _interval = std::chrono::seconds(1);

    public:

        void
        act() override;

        void
        react(sbn::kernel* child) override;

        inline void
        fanout(uint_type rhs) noexcept {
            this->_fanout = rhs;
        }

        inline void
        allow(const ifaddr_type& rhs) {
            if (rhs) {
                this->_allowedifaddrs.emplace(rhs);
            }
        }

        inline void
        interval(std::chrono::milliseconds rhs) noexcept {
            this->_interval = rhs;
        }

    private:

        void
        send_timer();

        interface_address_set
        enumerate_ifaddrs();

        void
        update_ifaddrs();

        void
        add_ifaddr(const ifaddr_type& rhs);

        void
        remove_ifaddr(const ifaddr_type& rhs);

        /// forward the probe to an appropriate discoverer
        void forward_probe(probe* p);
        void forward_hierarchy_kernel(Hierarchy_kernel* p);
        void on_event(sbn::socket_pipeline_kernel* k);
        void report_status(Status_kernel* status);

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

    };

}

#endif // vim:filetype=cpp
