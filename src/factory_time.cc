#include <iostream>
#include <iomanip>
#include <chrono>
#include <factory/base.hh>

using namespace factory;

int main(int argc, char* argv[]) {
	std::cout << current_time_nano() << std::endl;
	return 0;
}
