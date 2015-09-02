#include <sysx/sharedmem.hh>
#include <factory/ext/shmembuf.hh>

using namespace factory;
using sysx::shared_mem;
using factory::components::basic_shmembuf;
using factory::components::Error;
#include "test.hh"

template<class T>
struct Test_shmem {

	typedef stdx::log<Test_shmem> this_log;
	typedef typename shared_mem<T>::size_type size_type;
	typedef shared_mem<T> shmem;

	static bool
	shmem_invariant(shmem& shm) {
		return shm.begin() != nullptr && shm.end() != nullptr;
	}

	void test_shmem() {
		const typename shared_mem<T>::size_type SHMEM_SIZE = 512;
		const char SHMEMPROJID = 'F';
		shared_mem<T> mem1("/test-shmem", SHMEM_SIZE, 0666, SHMEMPROJID);
		shared_mem<T> mem2("/test-shmem", SHMEMPROJID);
		test::invar(shmem_invariant, mem1);
		test::invar(shmem_invariant, mem2);
		size_type real_size = mem1.size();
		std::cout << "mem1: " << mem1 << std::endl;
		std::cout << "mem2: " << mem2 << std::endl;
		test::equal(mem1.size(), real_size);
		test::equal(mem2.size(), real_size);
		mem2.sync();
		test::equal(mem2.size(), real_size);
		mem1.resize(real_size * 2);
		size_type new_size = mem1.size();
		test::equal(mem1.size(), new_size);
		mem2.sync();
		test::equal(mem2.size(), new_size);
		std::generate(mem1.begin(), mem1.end(), test::randomval<T>);
		test::compare(mem1, mem2);
	}
	
	void test_shmembuf() {
		typedef basic_shmembuf<T> shmembuf;
		shmembuf buf1("/test-shmem-2", 0600);
		shmembuf buf2("/test-shmem-2");
		std::vector<T> input(20);
		std::generate(input.begin(), input.end(), test::randomval<T>);
		buf1.lock();
		buf1.sputn(&input.front(), input.size());
		buf1.unlock();
		this_log() << "test_shmembuf() middle" << std::endl;
		std::vector<T> output(input.size());
		buf2.lock();
		buf2.sgetn(&output.front(), output.size());
		buf2.unlock();
		test::compare(input, output);
	}

};

int main(int argc, char* argv[]) {
	Test_shmem<char> test;
	test.test_shmem();
	test.test_shmembuf();
	return 0;
}
