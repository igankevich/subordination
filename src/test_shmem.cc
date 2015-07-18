#include <factory/factory_base.hh>
using namespace factory;
#include "test.hh"

namespace factory {

	template<class T>
	struct basic_shmembuf: public std::basic_streambuf<T> {

		using typename std::basic_streambuf<T>::int_type;
		using typename std::basic_streambuf<T>::traits_type;
		using typename std::basic_streambuf<T>::char_type;
		using typename std::basic_streambuf<T>::pos_type;
		using typename std::basic_streambuf<T>::off_type;
		typedef std::ios_base::openmode openmode;
		typedef std::ios_base::seekdir seekdir;
		typedef typename shared_mem<char_type>::size_type size_type;
		typedef typename shared_mem<char_type>::id_type id_type;
		typedef Spin_mutex mutex_type;
		typedef std::lock_guard<mutex_type> lock_type;
		typedef typename shared_mem<T>::proj_id_type proj_id_type;

		explicit basic_shmembuf(id_type id):
			_sharedmem(id, 512, BUFFER_PROJID),
			_sharedpart(new (this->_sharedmem.ptr()) shmem_header)
		{
			this->_sharedpart->size = this->_sharedmem.size();
			char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
			this->setg(ptr, ptr, ptr);
			this->setp(ptr, this->_sharedmem.end());
			this->debug("basic_shmembuf()");
		}

		explicit basic_shmembuf(id_type id, int):
			_sharedmem(id, BUFFER_PROJID),
			_sharedpart(new (this->_sharedmem.ptr()) shmem_header)
		{
			char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
			this->setg(ptr, ptr, ptr);
			this->setp(ptr, this->_sharedmem.end());
			this->debug("basic_shmembuf(int)");
		}

		~basic_shmembuf() = default;

		int_type overflow(int_type c = traits_type::eof()) {
			lock_type lock(this->_sharedpart->mtx);
			this->sync_sharedmem();
			int_type ret;
			if (c != traits_type::eof()) {
				this->grow_sharedmem();
				*this->pptr() = c;
				this->pbump(1);
				this->setg(this->eback(), this->gptr(), this->egptr()+1);
				this->writeoffs();
				ret = traits_type::to_int_type(c);
			} else {
				ret = traits_type::eof();
			}
			return ret;
		}

		int_type underflow() {
			lock_type lock(this->_sharedpart->mtx);
			return this->gptr() == this->egptr()
				? traits_type::eof()
				: traits_type::to_int_type(*this->gptr());
		}

		int_type uflow() {
			this->sync_sharedmem();
			if (this->underflow() == traits_type::eof()) return traits_type::eof();
			int_type c = traits_type::to_int_type(*this->gptr());
			this->gbump(1);
			this->writeoffs();
			return c;
		}

		std::streamsize xsputn(const char_type* s, std::streamsize n) {
			if (this->epptr() == this->pptr()) {
				this->overflow();
			}
			this->sync_sharedmem();
			std::streamsize avail = static_cast<std::streamsize>(this->epptr() - this->pptr());
			std::streamsize m = std::min(n, avail);
			traits_type::copy(this->pptr(), s, m);
			this->pbump(m);
			this->writeoffs();
		}

	private:

		void debug(const char* msg) {
			Logger<Level::SHMEM>()
				<< msg << ": size1="
				<< this->_sharedpart->size
				<< ",goff=" << this->_sharedpart->goff
				<< ",poff=" << this->_sharedpart->poff
				<< ",shmem="
				<< this->_sharedmem
				<< std::endl;
		}

		void grow_sharedmem() {
			this->_sharedmem.resize(this->_sharedmem.size()
				* size_type(2));
			this->_sharedpart->size = this->_sharedmem.size();
			this->updatebufs();
		}
		
		void sync_sharedmem() {
			// update shared memory size
			if (this->bad_size()) {
				this->_sharedmem.sync();
//				this->_sharedpart->size = this->_sharedmem.size();
			}
			this->debug("sync_sharedmem");
			if (this->bad_size()) {
				throw Error("bad shared_mem size",
					__FILE__, __LINE__, __func__);
			}
			this->readoffs();
		}
		
		bool bad_size() const {
			return this->_sharedpart->size
				!= this->_sharedmem.size();
		}
		
		void updatebufs() {
			std::ptrdiff_t poff = this->pptr() - this->pbase();
			std::ptrdiff_t goff = this->gptr() - this->eback();
			char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
			char_type* end = this->_sharedmem.end();
			this->setp(base, end);
			this->pbump(poff);
			this->setg(base, base + goff, base + poff);
		}

		void writeoffs() {
			this->_sharedpart->goff = static_cast<pos_type>(this->gptr() - this->eback());
			this->_sharedpart->poff = static_cast<pos_type>(this->pptr() - this->pbase());
			this->debug("writeoffs");
		}

		void readoffs() {
			pos_type goff = this->_sharedpart->goff;
			pos_type poff = this->_sharedpart->poff;
			char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
			char_type* end = this->_sharedmem.end();
			this->setp(base, end);
			this->pbump(poff);
			this->setg(base, base + goff, base + poff);
			this->debug("readoffs");
		}

		shared_mem<char_type> _sharedmem;
		struct shmem_header {
			size_type size;
			mutex_type mtx;
			pos_type goff;
			pos_type poff;
		} *_sharedpart;

		static const proj_id_type HEADER_PROJID = 'h';
		static const proj_id_type BUFFER_PROJID = 'b';
	};

}

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
