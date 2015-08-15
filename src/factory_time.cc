#include <libfactory.cc>

using namespace factory;

int main(int argc, char* argv[]) {
	std::cout << current_time_nano() << std::endl;
	using components::Ifaddrs;
	using components::Error;
	Ifaddrs addrs;
	std::for_each(addrs.begin(), addrs.end(), [] (const Ifaddrs::value_type& rhs) {
		if (rhs.ifa_addr == NULL || rhs.ifa_addr->sa_family != AF_INET) {
			return;
		}
		unix::endpoint addr(*rhs.ifa_addr);
		std::cout << std::setw(10) << std::left
			<< rhs.ifa_name << addr << std::endl;
	});
	std::cout << "Bind address: "
		<< components::get_bind_address()
		<< std::endl;
	std::cout << "bits::Bytes = " << bits::make_bytes(0xabcdefUL) << std::endl;
	throw Error("abc", __FILE__, __LINE__, __func__);
	return 0;
}
