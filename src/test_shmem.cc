#include "libfactory.cc"
using namespace factory;
using factory::components::shared_mem;
using factory::components::basic_shmembuf;
using factory::components::Error;
#include "test.hh"

template<class T>
struct Test_shmem {

	typedef stdx::log<Test_shmem> this_log;

	void test_shmem() {
		const typename shared_mem<T>::size_type SHMEM_SIZE = 512;
		const typename shared_mem<T>::proj_id_type SHMEMPROJID = 'F';
		shared_mem<T> mem1("/test-shmem", SHMEM_SIZE, 0666, SHMEMPROJID);
		shared_mem<T> mem2("/test-shmem", SHMEMPROJID);
		test::equal(mem1.size(), SHMEM_SIZE);
		test::equal(mem2.size(), SHMEM_SIZE);
		mem2.sync();
		test::equal(mem2.size(), SHMEM_SIZE);
		mem1.resize(SHMEM_SIZE * 2);
		test::equal(mem1.size(), SHMEM_SIZE * 2);
		mem2.sync();
		test::equal(mem2.size(), SHMEM_SIZE * 2);
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
	Test_shmem test;
	test.test_shmem<char>();
	test.test_shmembuf<char>();
	return 0;
}
