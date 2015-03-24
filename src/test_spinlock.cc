#include <factory/factory.hh>
using namespace factory;

void test_spinlock(int nthreads, int increment) {
	volatile uint64_t counter = 0;
	std::vector<std::thread> threads;
	Spin_mutex m;
	for (int i=0; i<nthreads; ++i) {
		threads.push_back(std::thread([&counter, increment, &m] () {
			for (int j=0; j<increment; ++j) {
				std::lock_guard<Spin_mutex> lock(m);
				++counter;
			}
		}));
	}
	for (std::thread& t : threads) {
		t.join();
	}
	uint64_t good_counter = nthreads*increment;
	if (counter != good_counter) {
		std::stringstream msg;
		msg << "Bad counter: " << counter << "/=" << good_counter;
		throw std::runtime_error(msg.str());
	}
}

int main() {
	try {
		int npowers = 16;
		unsigned min_threads = 2;
		unsigned max_threads = std::max(std::thread::hardware_concurrency(), 2*min_threads);
		for (unsigned j=min_threads; j<=max_threads; ++j) {
			for (int i=0; i<npowers; ++i) {
				test_spinlock(j, 1 << i);
			}
		}
	} catch (std::exception& e) {
		std::cerr << "Error. " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
