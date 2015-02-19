#include <ifaddrs.h>

namespace factory {

void find() {
	char host[NI_MAXHOST];

	struct ::ifaddrs *ifaddr;
	check("getifaddrs()", ::getifaddrs(&ifaddr));

	for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

		// ignore localhost and non-IPv4 addresses
		if (ifa->ifa_addr == NULL
			|| htonl(((struct ::sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr) == 0x7f000001ul
			|| ifa->ifa_addr->sa_family != AF_INET)
		{
			continue;
		}

		uint32_t addr_long = htonl(((struct ::sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr);
		uint32_t mask_long = htonl(((struct ::sockaddr_in*) ifa->ifa_netmask)->sin_addr.s_addr);

		uint32_t start = (addr_long & mask_long) + 1;
		uint32_t end = (addr_long & mask_long) + (~mask_long);

//		printf("\t\taddress: <%s>\n", host);
//		printf("\t\tnetmask: <%s>\n", netmask);
//		printf("\t\taddr_long: <%x>\n", addr_long);
//		printf("\t\tmask_long: <%x>\n", mask_long);
//		printf("\t\tshrt_addr: <%x>\n", addr_long & ~mask_long);
//		printf("\t\tstrt_addr: <%x>\n", start);
//		printf("\t\tend__addr: <%x>\n", end);

		for (uint32_t i=start; i<end; ++i) {
			struct ::sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = ntohl(i);
			addr.sin_port = 0;
			check("getnameinfo", ::getnameinfo((struct ::sockaddr*)&addr,
				sizeof(struct ::sockaddr_in),
				host, NI_MAXHOST,
				NULL, 0, NI_NUMERICHOST));
			std::cout << host << '\n';
		}
		
	}

	::freeifaddrs(ifaddr);
}

}
