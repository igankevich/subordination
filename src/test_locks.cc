#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <iostream>

#include <stdx/spin_mutex.hh>
#include <sysx/semaphore.hh>

#include "test.hh"
using namespace factory;

template<class Mutex>
void
test_counter(unsigned nthreads, unsigned increment) {
	volatile unsigned counter = 0;
	Mutex m;
	std::vector<std::thread> threads;
	for (unsigned i=0; i<nthreads; ++i) {
		threads.push_back(std::thread([&counter, increment, &m] () {
			for (unsigned j=0; j<increment; ++j) {
				std::lock_guard<Mutex> lock(m);
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
void run_multiple_times(Func func, I min_threads, I npowers) {
	I max_threads = std::max(I(std::thread::hardware_concurrency()), I(2)*min_threads);
	for (I j=min_threads; j<=max_threads; ++j) {
		for (I i=0; i<npowers; ++i) {
			func(j, I(1) << i);
		}
	}
}

template<class Q, class Mutex, class Semaphore=std::condition_variable>
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

template<class I, class Mutex, class Semaphore=std::condition_variable>
void test_queue(I nthreads, I max) {
	typedef Thread_pool<I, Mutex, Semaphore> Pool;
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

template<class Integer>
void test_perf_x(Integer m) {
	Time t0 = current_time_nano();
	run_multiple_times<Integer>(test_queue<Integer, stdx::spin_mutex>, 1, m);
	run_multiple_times<Integer>(test_queue<Integer, stdx::spin_mutex>, 1, m);
	Time t1 = current_time_nano();
	run_multiple_times<Integer>(test_queue<Integer, std::mutex>, 1, m);
	run_multiple_times<Integer>(test_queue<Integer, std::mutex>, 1, m);
	Time t2 = current_time_nano();
	std::cout << "Time(stdx::spin_mutex, " << m << ") = " << t1-t0 << "ns" << std::endl;
	std::cout << "Time(std::mutex, " << m << ") = " << t2-t1 << "ns" << std::endl;
}

template<class Mutex, class Integer=uint64_t>
void
test_perf(Integer m) {
	Time t0 = current_time_nano();
	run_multiple_times<Integer>(test_queue<Integer, Mutex>, 1, m);
	run_multiple_times<Integer>(test_queue<Integer, Mutex>, 1, m);
	Time t1 = current_time_nano();
	std::cout
		<< "mutex=" << typeid(Mutex).name()
		<< ", int_type=" << typeid(Integer).name()
		<< ", time=" << t1-t0 << "ns"
		<< std::endl;
}

struct self_signal_semaphore: public sysx::signal_semaphore {
	self_signal_semaphore():
	sysx::signal_semaphore(sysx::this_process::id(), SIGUSR1)
	{}
};

int main() {
	sysx::init_signal_semaphore init(SIGUSR1);
	try {
		run_multiple_times<unsigned>(test_counter<stdx::spin_mutex>, 2, 10);
		test_perf<stdx::spin_mutex>(10);
		test_perf<std::mutex>(10);
		run_multiple_times<unsigned>(test_queue<unsigned,std::mutex,self_signal_semaphore>, 1, 10);
//		run_multiple_times<unsigned>(test_counter<bits::global_mutex>, 2, 2);
	} catch (std::exception& e) {
		std::cerr << "Error. " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
