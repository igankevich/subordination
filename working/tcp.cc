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

#define BUFFER_MAX_SIZE 1024
int receive_reply(Socket socket) {
	/* Other code here */
	/* .... */
	/* Handle receving ICMP Errors */
	int return_status;
	char buffer[BUFFER_MAX_SIZE];
	struct iovec iov;                       /* Data array */
	struct msghdr msg;                      /* Message header */
	struct cmsghdr *cmsg;                   /* Control related data */
	struct sock_extended_err *sock_err;     /* Struct describing the error */
	struct icmphdr icmph;                   /* ICMP header */
	struct sockaddr_in remote;              /* Our socket */

	iov.iov_base = &icmph;
	iov.iov_len = sizeof(icmph);
	msg.msg_name = (void*)&remote;
	msg.msg_namelen = sizeof(remote);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	msg.msg_control = buffer;
	msg.msg_controllen = sizeof(buffer);
	
//	while(recvmsg(socket, &msg, 0) < 0);
	int ret = recvmsg(socket, &msg, 0);
//	int ret = check("recvmsg()", recvmsg(socket, &msg, 0));
	return ret;
	std::clog << msg.msg_controllen << std::endl;
//	check("recvmsg()", recvmsg(socket, &msg, MSG_ERRQUEUE));
	/* Control messages are always accessed via some macros
	 * http://www.kernel.org/doc/man-pages/online/pages/man3/cmsg.3.html
	 */
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		/* Ip level */
		std::cout
			<< "cmsg_level = " << SOL_IP << ", "
			<< "cmsg_type = " << IP_RECVERR << std::endl;
		if (cmsg->cmsg_level == SOL_IP and cmsg->cmsg_type == IP_RECVERR) {
			fprintf(stderr, "We got IP_RECVERR message\n");
			sock_err = (struct sock_extended_err*)CMSG_DATA(cmsg);
			if (sock_err and sock_err->ee_origin == SO_EE_ORIGIN_ICMP) {
				/* Handle ICMP errors types */
				switch (sock_err->ee_type) {
				case ICMP_NET_UNREACH:
					/* Hendle this error */
					fprintf(stderr, "Network Unreachable Error\n");
					break;
				case ICMP_HOST_UNREACH:
					/* Hendle this error */
					fprintf(stderr, "Host Unreachable Error\n");
					break;
					/* Handle all other cases. Find more errors :
					 * http://lxr.linux.no/linux+v3.5/include/linux/icmp.h#L39
					 */
				case ICMP_PORT_UNREACH:
					fprintf(stderr, "Port Unreachable Error\n");
					break;
				case ICMP_TIME_EXCEEDED: {
					Endpoint from((struct sockaddr_in*)SO_EE_OFFENDER(sock_err));
					struct icmphdr* icmp = (struct icmphdr*) ((char*)iov.iov_base + 20);
					std::cout << "From = " << from << std::endl;
					std::cout << "Code = " << (int)sock_err->ee_code << std::endl;
					break;
				}
				default:
					fprintf(stderr, "ICMP error %d\n", sock_err->ee_type);
				}
			}
		}
	}
}

int main (int argc, char *argv[]) {

	Endpoint src("0.0.0.0", 40000);
	Endpoint dest("172.27.221.184", 50000);

	Socket socket2;
	socket2.listen(dest);

	Socket socket;
//	socket.connect(dest);
//	socket.connect(Endpoint("213.180.204.3", 80));
//	socket.connect(Endpoint("lab1.apmath.spbu.ru", 80), UNRELIABLE_SOCKET);
	socket.connect(Endpoint("ant.apmath.spbu.ru", 22));
//	socket.connect(Endpoint("csa.ru", 80));



//	auto pair = socket2.accept();
//	int recv_socket = pair.first;

	sleep(1);

	socklen_t len = sizeof(errno);
	check("getsockopt()", ::getsockopt(socket, SOL_SOCKET, SO_ERROR, &errno, &len));
	std::cout << "errno = " << errno << std::endl;
	if (errno) {
		perror("Connection refused");
	}

	for (int ttl = 1; ttl < 64; ttl += 1) {

		check("setsockopt()", ::setsockopt(socket, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)));

		char msg[12] = "Hello world";
//		std::clog << "Sending" << std::endl;
		check("send()", ::send(socket, msg, sizeof(msg), 0));


//
//		sleep(1);
//		char msg2[12];
//		check("recv()", ::recv(recv_socket, msg2, sizeof(msg2), 0));
//		std::cout << "Message: " << msg2 << std::endl;

		sleep(1);
		int ret = receive_reply(socket);
		if (ret == -1) {
			perror("reply");
		}
		std::cout
			<< "ret = " << ret << ", "
			<< "TTL = " << ttl << std::endl;
		if (ret != -1) {
			break;
		}
	}

	return 0;
}




