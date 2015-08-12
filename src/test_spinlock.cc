#include <factory/factory_base.hh>
using namespace factory;

void spinlock_counter(unsigned nthreads, unsigned increment) {
	volatile unsigned counter = 0;
	spin_mutex m;
	std::vector<std::thread> threads;
	for (unsigned i=0; i<nthreads; ++i) {
		threads.push_back(std::thread([&counter, increment, &m] () {
			for (unsigned j=0; j<increment; ++j) {
				std::lock_guard<spin_mutex> lock(m);
				++counter;
			}
		}));
	}
	for (std::thread& t : threads) {
		t.join();
	}
	unsigned good_counter = nthreads*increment;
	if (counter != good_counter) {
		std::stringstream msg;
		msg << "Bad counter: " << counter << "/=" << good_counter;
		throw std::runtime_error(msg.str());
	}
}

template<class I, class Func>
void test_spinlock(Func func, I min_threads, I npowers) {
	I max_threads = std::max(I(std::thread::hardware_concurrency()), I(2)*min_threads);
	for (I j=min_threads; j<=max_threads; ++j) {
		for (I i=0; i<npowers; ++i) {
			func(j, I(1) << i);
		}
	}
}

template<class Q, class Mutex>
struct Thread_pool {

	Thread_pool():
		queue(),
		cv(),
		mtx(),
		thread([this] () {
		while (!stopped) {
			Q val;
			{
				std::lock_guard<Mutex> lock(mtx);
				cv.wait(mtx, [this] () { return stopped || !queue.empty(); });
				if (stopped) break;
				val = queue.front();
				queue.pop();
			}
			if (val == -1) {
				stopped = true;
			} else {
				sum += val;
			}
		}
	}) {}

	void submit(Q q) {
		{
			std::lock_guard<Mutex> lock(mtx);
			queue.push(q);
		}
		cv.notify_one();
	}

	void wait() {
		if (thread.joinable()) {
			thread.join();
		}
	}

	Q result() const { return sum; }
	
private:
	std::queue<Q> queue;
	std::condition_variable_any cv;
	Mutex mtx;
	volatile bool stopped = false;
	std::thread thread;
	Q sum = 0;
};

template<class I, class M>
void spinlock_queue(I nthreads, I max) {
	typedef Thread_pool<I, M> Pool;
	std::vector<Pool*> thread_pool(nthreads);
	std::for_each(thread_pool.begin(), thread_pool.end(), [] (Pool*& ptr) {
		ptr = new Pool;
	});
	I expected_sum = (max + I(1))*max/I(2);
	for (I i=1; i<=max; ++i) {
		thread_pool[i%thread_pool.size()]->submit(i);
	}
	for (Pool* pool : thread_pool) {
		pool->submit(I(-1));
	}
	std::for_each(thread_pool.begin(), thread_pool.end(),
		std::mem_fun(&Pool::wait));
	I sum = std::accumulate(thread_pool.begin(), thread_pool.end(), I(0),
		[] (I sum, Pool* ptr) {
			return sum + ptr->result();
		}
	);
//	for (Pool* pool : thread_pool) {
//		std::cout << pool->result() << std::endl;
//	}
//	std::cout << max << ": " << sum << std::endl;
	std::for_each(thread_pool.begin(), thread_pool.end(), [] (Pool* ptr) {
		delete ptr;
	});
	if (sum != expected_sum) {
		std::stringstream msg;
		msg << "Bad sum: " << sum << "/=" << expected_sum;
		throw std::runtime_error(msg.str());
	}
}

template<class Bigint>
void test_perf(Bigint m) {
	Time t0 = current_time_nano();
	test_spinlock<Bigint>(spinlock_queue<Bigint, spin_mutex>, 1, m);
	test_spinlock<Bigint>(spinlock_queue<Bigint, spin_mutex>, 1, m);
	Time t1 = current_time_nano();
	test_spinlock<Bigint>(spinlock_queue<Bigint, std::mutex>, 1, m);
	test_spinlock<Bigint>(spinlock_queue<Bigint, std::mutex>, 1, m);
	Time t2 = current_time_nano();
	std::cout << "Time(spin_mutex, " << m << ") = " << t1-t0 << "ns" << std::endl;
	std::cout << "Time(std::mutex, " << m << ") = " << t2-t1 << "ns" << std::endl;
}

int main() {
	try {
		test_spinlock<unsigned>(spinlock_counter, 2, 10);
		test_perf<uint32_t>(10);
		test_perf<uint64_t>(10);
	} catch (std::exception& e) {
		std::cerr << "Error. " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
