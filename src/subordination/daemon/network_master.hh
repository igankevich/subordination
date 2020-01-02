#ifndef SUBORDINATION_DAEMON_NETWORK_MASTER_HH
#define SUBORDINATION_DAEMON_NETWORK_MASTER_HH

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <unistdx/net/interface_addresses>

#include <subordination/api.hh>
#include <subordination/daemon/master_discoverer.hh>
#include <subordination/ppl/socket_pipeline_event.hh>

namespace bsc {

    class network_timer: public bsc::kernel {};

    class network_master: public bsc::kernel {

    private:
        typedef sys::ipv4_address addr_type;
        typedef addr_type::rep_type uint_type;
        typedef sys::interface_address<addr_type> ifaddr_type;
        typedef typename sys::ipaddr_traits<addr_type> traits_type;
        typedef std::unordered_set<ifaddr_type> set_type;
        typedef std::unordered_map<ifaddr_type,master_discoverer*> map_type;
        typedef typename map_type::iterator map_iterator;

    private:
        map_type _ifaddrs;
        set_type _allowedifaddrs;
        uint_type _fanout = 10000;
        network_timer* _timer = nullptr;
        /// Interface address list update interval.
        std::chrono::milliseconds _interval = std::chrono::seconds(1);

    public:

        void
        act() override;

        void
        react(bsc::kernel* child) override;

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

        set_type
        enumerate_ifaddrs();

        void
        update_ifaddrs();

        void
        add_ifaddr(const ifaddr_type& rhs);

        void
        remove_ifaddr(const ifaddr_type& rhs);

        /// forward the probe to an appropriate discoverer
        void
        forward_probe(probe* p);

        void
        forward_hierarchy_kernel(hierarchy_kernel* p);

        map_iterator
        find_discoverer(const addr_type& a);

        void
        on_event(socket_pipeline_kernel* k);

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
