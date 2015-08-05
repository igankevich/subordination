#include <factory/factory_base.hh>
using namespace factory;
#include "test.hh"

template<class T>
void test_shmem() {
	const typename shared_mem<T>::size_type SHMEM_SIZE = 512;
	const typename shared_mem<T>::proj_id_type SHMEMPROJID = 'F';
	shared_mem<T> mem1(this_process::id(), SHMEM_SIZE, SHMEMPROJID);
	shared_mem<T> mem2(this_process::id(), SHMEMPROJID);
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

template<class T>
void test_shmembuf() {
	typedef basic_shmembuf<T> shmembuf;
	shmembuf buf1(this_process::id());
	shmembuf buf2(this_process::id(), 0);
	std::vector<T> input(20);
	std::generate(input.begin(), input.end(), test::randomval<T>);
	buf1.sputn(&input.front(), input.size());
	Logger<Level::TEST>() << "test_shmembuf() middle" << std::endl;
	std::vector<T> output(input.size());
	buf2.sgetn(&output.front(), output.size());
	test::compare(input, output);
}

//struct Allocator {
//};
//
//
//template<class T>
//struct Shmem_queue {
//
//	typedef Spin_mutex Mutex;
////	typedef std::mutex Mutex;
//	typedef std::condition_variable Cond;
//
//	struct Header {
//		uint16_t head = 0;
//		uint16_t tail = 0;
//		Mutex mutex;
//		char padding[3];
//	};
//
////	static_assert(sizeof(Header) == 8,
////		"Bad Shmem_queue header size.");
//	
//	Shmem_queue(int id, size_t max_elems):
//		shmem(id, sizeof(T)*max_elems + sizeof(Header)),
//		header(new (shmem.ptr()) Header),
//		first(new (shmem.ptr() + sizeof(Header)) T[max_elems]),
//		last(first + max_elems)
//	{
//		std::cout << "sizeof " << sizeof(Header) << std::endl;
//		std::cout << "sizeof " << sizeof(Mutex) << std::endl;
//		std::cout << "sizeof " << sizeof(std::condition_variable) << std::endl;
//		std::cout << "sizeof " << sizeof(std::condition_variable_any) << std::endl;
//		std::cout << "sizeof " << sizeof(std::mutex) << std::endl;
//	}
//
//	explicit Shmem_queue(int id):
//		shmem(id),
//		header(reinterpret_cast<Header*>(shmem.ptr())),
//		first(reinterpret_cast<T*>(shmem.ptr() + sizeof(Header))),
//		last(first + (shmem.size() - sizeof(Header)) / sizeof(T))
//	{}
//
//	void push(const T& val) {
//		first[header->tail++] = val;
//	}
//
//	size_t write(const T* buf, size_t size) {
//		size_t sz = std::min(size, first + header->tail - last);
//		std::copy_n(buf, sz, first + header->tail);
//		return sz;
//	}
//
//	size_t read(T* buf, size_t size) {
//		size_t sz = std::min(size, header->tail - header->head);
//		std::copy_n(first + header->head, sz, buf);
//		return sz;
//	}
//
//	void pop() { header->head++; }
//	T& front() { return first[header->head]; }
//	const T& front() const { return first[header->head]; }
//	uint32_t size() const { return header->tail - header->head; }
//	bool empty() const { return size() == 0; }
//	Mutex& mutex() { return header->mutex; }
//	
//private:
//
//	friend std::ostream& operator<<(std::ostream& out, const Shmem_queue& rhs) {
//		out << "shmem = " << rhs.shmem << ", head = ";
//		if (rhs.header) {
//			out << rhs.header->head;
//		} else {
//			out << "none";
//		}
//		return out;
//	}
//
//	shared_mem shmem;
//	Header* header;
//	T* first;
//	T* last;
//};
//
//template<class T>
//struct Shmem_server {
//
//	typedef typename Shmem_queue<T>::Mutex Mutex;
//	typedef typename Shmem_queue<T>::Cond Cond;
//
//	explicit Shmem_server(int id):
//		_pool(id), _semaphore(gen_name(id)) {}
//
//	Shmem_server(int id, size_t size):
//		_pool(id, size), _semaphore(gen_name(id)),
//		worker([this] () { serve(); }) {}
//
//	void send(T val) {
//		std::lock_guard<Mutex> lock(_pool.mutex());
//		_pool.push(val);
//		_semaphore.notify_one();
//		std::cout << "Send " << val << std::endl;
////		if (val == 'q') {
////			stopped = true;
////		}
//	}
//
//	void wait() {
//		if (worker.joinable()) {
//			worker.join();
//		}
//	}
//
//private:
//
//	static std::string gen_name(int id) {
//		std::stringstream s;
//		s << "factory-" << id;
//		return s.str();
//	}
//
//	void serve() {
//		while (!stopped) {
//			std::cout << "QQ " << std::endl;
//			while (!(stopped || !_pool.empty())) {
//				_semaphore.wait();
//			}
//			std::lock_guard<Mutex> lock(_pool.mutex());
//			std::cout << "QQ1" << std::endl;
//			if (stopped) { break; }
//			std::cout << "QQ2" << std::endl;
//			T val = _pool.front();
//			_pool.pop();
//			std::cout << "Recv " << val << std::endl;
//			if (val == 'q') {
//				stopped = true;
//			}
//		}
//		std::cout << "Exiting " << std::endl;
//	}
//	
//	Shmem_queue<T> _pool;
//	Semaphore _semaphore;
//	volatile bool stopped = false;
//	std::thread worker;
//};
//
//
//void test_shmem_client() {
//	Shmem_server<char> queue(this_process::parent_id());
//	sleep(2);
//	queue.send('a');
//	queue.send('q');
//	sleep(2);
//	queue.wait();
//}
//
//void test_shmem_server(char** argv) {
//	Shmem_server<char> queue(this_process::id(), 1024);
//	Process_group procs;
//	procs.add([&argv] () {
//		return this_process::execute(argv[0], "client");
//	});
//	queue.wait();
//	procs.wait();
//}
//
//struct App {
//	int run(int argc, char** argv) {
//		try {
//			if (argc == 1) {
//				test_shmem_server(argv);
//			} else {
//				Command_line cmdline(argc, argv);
//				cmdline.parse([&argv] (std::string& arg, std::istream& cmdline) {
//					if (arg == "client") test_shmem_client();
//					else test_shmem_server(argv);
//				});
//			}
//		} catch (std::exception& e) {
//			std::cerr << e.what() << std::endl;
//			return 1;
//		}
//		return 0;
//	}
//};

int main(int argc, char* argv[]) {
	test_shmem<char>();
	test_shmembuf<char>();
	return 0;
}
