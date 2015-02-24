#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<pthread.h>
#include<poll.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<signal.h>
#include<sys/sem.h>
#include<poll.h>
#include<pthread.h>
#include<sys/select.h>
#include<sys/un.h>
#define SA (struct sockaddr*)
//#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include <linux/errqueue.h>

#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <chrono>
#include <netdb.h>
#include "../src/factory/error.hh"
#include "../src/factory/endpoint.hh"
#include "../src/factory/socket.hh"


unsigned short csum (unsigned short *buf, int nwords) {
	unsigned long sum;
	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

using namespace factory;

struct Header {
	struct ip ip_hdr;
	struct icmphdr icmp;
};

int main (int argc, char *argv[]) {

	if (argc != 2) {
		std::cerr << "USAGE: ./udp <addr>" << std::endl;
		return 1;
	}

	int socket = check("socket()", ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP));
//
//	Socket socket2;
//	socket2.listen(dest, UNRELIABLE_SOCKET);
	int on = 1;
	check("setsockopt()", ::setsockopt(socket, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)));
	
	Endpoint dest(argv[1], 0);

	for (int ttl = 1; ttl < 9; ++ttl) {

		check("setsockopt()", ::setsockopt(socket, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)));

		Header hdr;
		hdr.ip_hdr.ip_hl = 5;
		hdr.ip_hdr.ip_v = 4;
		hdr.ip_hdr.ip_tos = 0;
		hdr.ip_hdr.ip_len = 20 + 8;
		hdr.ip_hdr.ip_id = 10000;
		hdr.ip_hdr.ip_off = 0;
		hdr.ip_hdr.ip_ttl = ttl;
		hdr.ip_hdr.ip_p = IPPROTO_ICMP;
		::inet_pton(AF_INET, "172.27.221.184", &(hdr.ip_hdr.ip_src));
		::inet_pton(AF_INET, argv[1], &(hdr.ip_hdr.ip_dst));
		hdr.ip_hdr.ip_sum = csum ((unsigned short *) &hdr, 9);

		hdr.icmp.type = ICMP_ECHO;
		hdr.icmp.code = 0;
		hdr.icmp.checksum = 0;
		hdr.icmp.un.echo.id = 0;
		hdr.icmp.un.echo.sequence = ttl + 1;
		hdr.icmp.checksum = csum ((unsigned short *) (&hdr + 20), 4);

		char msg[12] = "Hello world";
		check("sendto()", ::sendto(socket, (char*)&hdr, sizeof(hdr), 0,
			(struct ::sockaddr*) dest.addr(), sizeof(sockaddr_in)));

		struct ::sockaddr_in addr;
		socklen_t len = 0;
		Header reply;
		check("recvfrom()", ::recvfrom(socket, (char*)&reply, sizeof(reply), 0,
			(struct ::sockaddr*) &addr, &len));

		std::cout
			<< Endpoint(&addr) << ", "
			<< "Type = " << (int)hdr.icmp.type << ", "
			<< "Code = " << (int)hdr.icmp.code << std::endl;
	}

	::close(socket);

	return 0;
}




