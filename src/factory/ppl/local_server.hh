#ifndef FACTORY_PPL_LOCAL_SERVER_HH
#define FACTORY_PPL_LOCAL_SERVER_HH

#include <unistdx/net/endpoint>
#include <unistdx/net/ifaddr>

namespace factory {

	template <class Addr>
	class local_server {

	public:
		typedef Addr addr_type;
		typedef sys::ifaddr<addr_type> ifaddr_type;

	private:
		ifaddr_type _ifaddr;

	public:
		inline explicit
		local_server(const ifaddr_type& ifaddr):
		_ifaddr(ifaddr)
		{}

	};

}

#endif // vim:filetype=cpp
