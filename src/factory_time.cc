#include <libfactory.cc>

using namespace factory;

int main(int argc, char* argv[]) {
	std::cout << current_time_nano() << std::endl;
	throw Error("abc", __FILE__, __LINE__, __func__);
	return 0;
}
