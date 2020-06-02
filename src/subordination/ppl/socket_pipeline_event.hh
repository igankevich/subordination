#ifndef SUBORDINATION_PPL_SOCKET_PIPELINE_EVENT_HH
#define SUBORDINATION_PPL_SOCKET_PIPELINE_EVENT_HH

#include <cassert>

#include <unistdx/net/interface_address>
#include <unistdx/net/ipv4_address>

namespace sbn {

    enum class socket_pipeline_event {
        add_client,
        add_server,
        remove_client,
        remove_server,
    };

    class socket_pipeline_kernel: public kernel {

    public:
        typedef sys::ipv4_address addr_type;
        typedef sys::interface_address<addr_type> ifaddr_type;

    private:
        socket_pipeline_event _event = socket_pipeline_event(0);
        ifaddr_type _ifaddr;
        sys::socket_address _endpoint;

    public:
        socket_pipeline_kernel() = default;
        socket_pipeline_kernel(const socket_pipeline_kernel&) = default;

        inline
        socket_pipeline_kernel(
            socket_pipeline_event event,
            const ifaddr_type& interface_address
        ):
        _event(event),
        _ifaddr(interface_address) {
            assert(
                event == socket_pipeline_event::add_server ||
                event == socket_pipeline_event::remove_server
            );
        }

        inline
        socket_pipeline_kernel(
            socket_pipeline_event event,
            const sys::socket_address& socket_address
        ):
        _event(event),
        _endpoint(socket_address)
        {
            assert(
                event == socket_pipeline_event::add_client ||
                event == socket_pipeline_event::remove_client
            );
        }

        inline socket_pipeline_event
        event() const noexcept {
            return this->_event;
        }

        inline const sys::socket_address&
        socket_address() const noexcept {
            return this->_endpoint;
        }

        inline const ifaddr_type&
        interface_address() const noexcept {
            return this->_ifaddr;
        }

    };

}

#endif // vim:filetype=cpp
