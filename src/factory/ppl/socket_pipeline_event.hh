#ifndef FACTORY_PPL_SOCKET_PIPELINE_EVENT_HH
#define FACTORY_PPL_SOCKET_PIPELINE_EVENT_HH

#include <cassert>

#include <factory/config.hh>

#include <unistdx/net/ifaddr>
#include <unistdx/net/ipv4_addr>

namespace factory {

	enum class socket_pipeline_event {
		add_client,
		add_server,
		remove_client,
		remove_server,
	};

	class socket_pipeline_kernel: public FACTORY_KERNEL_TYPE {

	public:
		typedef sys::ipv4_addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;

	private:
		socket_pipeline_event _event = socket_pipeline_event(0);
		ifaddr_type _ifaddr;
		sys::endpoint _endpoint;

	public:
		socket_pipeline_kernel() = default;
		socket_pipeline_kernel(const socket_pipeline_kernel&) = default;

		inline
		socket_pipeline_kernel(
			socket_pipeline_event event,
			const ifaddr_type& ifaddr
		):
		_event(event),
		_ifaddr(ifaddr) {
			assert(
				event == socket_pipeline_event::add_server ||
				event == socket_pipeline_event::remove_server
			);
		}

		inline
		socket_pipeline_kernel(
			socket_pipeline_event event,
			const sys::endpoint& endpoint
		):
		_event(event),
		_endpoint(endpoint)
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

		inline const sys::endpoint&
		endpoint() const noexcept {
			return this->_endpoint;
		}

		inline const ifaddr_type&
		ifaddr() const noexcept {
			return this->_ifaddr;
		}

	};

}

#endif // vim:filetype=cpp
