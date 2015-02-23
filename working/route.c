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
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>


unsigned short csum (unsigned short *buf, int nwords) {
	unsigned long sum;
	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

int main (int argc, char *argv[]) {
	if (argc != 2) {
		printf ("need destination for tracert\n");
		exit (0);
	}
	int sfd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
	char buf[4096] = { 0 };
	struct ip *ip_hdr = (struct ip *) buf;
	int hop = 0;

	int one = 1;
	if (setsockopt (sfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof (one)) < 0)
		printf ("Cannot set IP_HDRINCL!\n");
//	if (setsockopt (sfd, SOL_IP, IP_RECVERR, &one, sizeof (one)) < 0)
//		printf ("Cannot set IP_RECVERR!\n");

	struct sockaddr_in addr;
	addr.sin_port = htons (7);
	addr.sin_family = AF_INET;
	inet_pton (AF_INET, argv[1], &(addr.sin_addr));


	while (1) {
		ip_hdr->ip_hl = 5;
		ip_hdr->ip_v = 4;
		ip_hdr->ip_tos = 0;
		ip_hdr->ip_len = 20 + 8;
		ip_hdr->ip_id = 10000;
		ip_hdr->ip_off = 0;
		ip_hdr->ip_ttl = hop;
		ip_hdr->ip_p = IPPROTO_ICMP;
		inet_pton (AF_INET, "192.168.1.20", &(ip_hdr->ip_src));
		inet_pton (AF_INET, argv[1], &(ip_hdr->ip_dst));
		ip_hdr->ip_sum = csum ((unsigned short *) buf, 9);

		struct icmphdr *icmphd = (struct icmphdr *) (buf + 20);
		icmphd->type = ICMP_ECHO;
		icmphd->code = 0;
		icmphd->checksum = 0;
		icmphd->un.echo.id = 0;
		icmphd->un.echo.sequence = hop + 1;
		icmphd->checksum = csum ((unsigned short *) (buf + 20), 4);
		if (sendto(sfd, buf, sizeof(struct ip) + sizeof(struct icmphdr), 0, SA & addr, sizeof addr) == -1) {
			perror("sendto");
		}
		char buff[4096] = { 0 };
		struct sockaddr_in addr2;
		socklen_t len = sizeof (struct sockaddr_in);
		if (recvfrom (sfd, buff, sizeof(buff), 0, SA & addr2, &len) == -1) {
			perror("recvfrom");
		}
//
//		int errno = 0;
//		socklen_t errlen = sizeof(errno);
//		getsockopt(sfd, SOL_SOCKET, SO_ERROR, &errno, &errlen);
//		printf("errno = %i\n", errno);

		struct icmphdr *icmphd2 = (struct icmphdr *) (buff + 20);
		if (icmphd2->code != 0) {
			printf("type = %i, code = %i\n", icmphd2->type, icmphd2->code);
			break;
		}
		if (icmphd2->type != 0)
			printf ("hop limit:%d Address:%s, code = %d\n", hop, inet_ntoa (addr2.sin_addr), icmphd2->code);
		else {
			printf ("Reached destination:%s with hop limit:%d\n",
					inet_ntoa (addr2.sin_addr), hop);
			exit (0);
		}

		hop++;
	}

	return 0;
}



