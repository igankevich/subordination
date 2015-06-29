#include <iostream>
#include <iomanip>
#include <chrono>
#include <factory/factory.hh>

using namespace factory;

int main(int argc, char* argv[]) {
	std::cout << current_time_nano() << std::endl;
	try {
		throw Error("abc", __FILE__, __LINE__, __func__);
	} catch (Error& err) {
		std::cerr << Error_message(err, __FILE__, __LINE__, __func__);
	}
	return 0;
}
