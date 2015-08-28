#include <sysx/packstream.hh>
#include <sstream>
#include <vector>
#include <random>

#include "datum.hh"
#include "test.hh"

std::random_device rng;

void test_marshaling() {
	const size_t size = 20;
	std::vector<Datum> input(size);
	std::vector<Datum> output(size);
	test::randomise(input.begin(), input.end(), rng);

	std::stringbuf buf;
	sysx::packstream stream(&buf);
	for (size_t i=0; i<input.size(); ++i)
		stream << input[i];

	for (size_t i=0; i<input.size(); ++i)
		stream >> output[i];
	
	test::compare(input, output);
}

int main(int argc, char* argv[]) {
	test_marshaling();
	return 0;
}
