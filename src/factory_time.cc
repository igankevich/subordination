#include "libfactory.cc"
#include "test.hh"
#include <sys/resource.h>

using namespace factory;

int main(int argc, char* argv[]) {
	std::cout << current_time_nano() << std::endl;
	using components::Error;
	sysx::ifaddrs addrs;
	std::for_each(addrs.begin(), addrs.end(), [] (const sysx::ifaddrs::value_type& rhs) {
		if (rhs.ifa_addr == NULL || rhs.ifa_addr->sa_family != AF_INET) {
			return;
		}
		sysx::endpoint addr(*rhs.ifa_addr);
		std::cout << std::setw(10) << std::left
			<< rhs.ifa_name << addr << std::endl;
	});
	std::cout << "bits::Bytes = " << bits::make_bytes(0xabcdefUL) << std::endl;
	std::clog << "[clog] Hello world!" << std::endl;
	::syslog(LOG_NOTICE, "[syslog] Hello world!");
	std::cerr << "[cerr] Hello world!" << std::endl;
//	sleep(3);
	struct ::rlimit limit;
	::getrlimit(RLIMIT_NOFILE, &limit);
	std::cout << "Max no. of fds: " << limit.rlim_max << std::endl;
	components::Daemon<Factory> __daemon(__factory);
//	throw Error("abc", __FILE__, __LINE__, __func__);
	return 0;
}
