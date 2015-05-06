#include <factory/factory.hh>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "process.hh"

using namespace factory;

struct Shared_memory {

	typedef unsigned char Byte;

	Shared_memory(int id, size_t sz, int perms = DEFAULT_SHMEM_PERMS):
		_addr(nullptr), _id(id), _size(sz), _shmid(0), _owner(true)
	{
		_path = filename(id);
		{ std::ofstream out(_path); }
		::key_t key = check("ftok()", ::ftok(_path.c_str(), PROJ_ID));
		_shmid = check("shmget()", ::shmget(key, size(), perms | IPC_CREAT));
		_addr = static_cast<Byte*>(check("shmat()", ::shmat(_shmid, 0, 0)));
	}

	explicit Shared_memory(int id):
		_id(id), _size(0), _shmid(0), _path()
	{
		_path = filename(id);
		{ std::ofstream out(_path); }
		::key_t key = check("ftok()", ::ftok(_path.c_str(), PROJ_ID));
		_shmid = check("shmget()", ::shmget(key, 0, 0));
		_addr = static_cast<Byte*>(check("shmat()", ::shmat(_shmid, 0, 0)));
		load_size();
	}

	~Shared_memory() {
		check("shmdt()", ::shmdt(_addr));
		if (is_owner()) {
			check("shmctl()", ::shmctl(_shmid, IPC_RMID, 0));
			check("remove()", ::remove(_path.c_str()));
		}
	}

	Shared_memory(const Shared_memory&) = delete;
	Shared_memory& operator=(const Shared_memory&) = delete;

	unsigned char* ptr() { return _addr; }
	const unsigned char* ptr() const { return _addr; }
	size_t size() const { return _size; }
	bool is_owner() const { return _owner; }

private:

	void load_size() {
		::shmid_ds stat;
		check("shmctl()", ::shmctl(_shmid, IPC_STAT, &stat));
		_size = stat.shm_segsz;
	}
	
	std::string filename(int id) const {
		std::stringstream path;
		path << "/var/tmp";
		path << "/factory.";
		path << id;
		path << ".shmem";
		return path.str();
	}

	friend std::ostream& operator<<(std::ostream& out, const Shared_memory& rhs) {
		return out << "addr = " << reinterpret_cast<const void*>(rhs.ptr())
			<< ", size = " << rhs.size()
			<< ", owner = " << rhs.is_owner()
			<< ", path = " << rhs._path;
	}

	Byte* _addr;
	int _id = 0;
	size_t _size = 0;
	int _shmid = 0;
	std::string _path;
	bool _owner = false;

	static const int DEFAULT_SHMEM_PERMS = 0666;
	static const int PROJ_ID = 'Q';
};

struct Semaphore {
	explicit Semaphore(const std::string& name):
		Semaphore(name.c_str()) {}
	explicit Semaphore(const char* name) {
		_sem = check("sem_open()", ::sem_open(name, O_CREAT, 0666, 0));
	}
	~Semaphore() { check("sem_close()", ::sem_close(_sem)); }
	void wait() { check("sem_wait()", ::sem_wait(_sem)); }
	void notify_one() { check("sem_post()", ::sem_post(_sem)); }

	void lock() { this->wait(); }
	void unlock() { this->notify_one(); }

private:
	::sem_t* _sem;
};

template<class T>
struct Shmem_queue {

	typedef Spin_mutex Mutex;
//	typedef std::mutex Mutex;
	typedef std::condition_variable Cond;

	struct Header {
		uint16_t head = 0;
		uint16_t tail = 0;
		Mutex mutex;
		char padding[3];
	};

//	static_assert(sizeof(Header) == 8,
//		"Bad Shmem_queue header size.");
	
	Shmem_queue(int id, size_t max_elems):
		shmem(id, sizeof(T)*max_elems + sizeof(Header)),
		header(new (shmem.ptr()) Header),
		first(new (shmem.ptr() + sizeof(Header)) T[max_elems]),
		last(first + max_elems)
	{
		std::cout << "sizeof " << sizeof(Header) << std::endl;
		std::cout << "sizeof " << sizeof(Mutex) << std::endl;
		std::cout << "sizeof " << sizeof(std::condition_variable) << std::endl;
		std::cout << "sizeof " << sizeof(std::condition_variable_any) << std::endl;
		std::cout << "sizeof " << sizeof(std::mutex) << std::endl;
	}

	explicit Shmem_queue(int id):
		shmem(id),
		header(reinterpret_cast<Header*>(shmem.ptr())),
		first(reinterpret_cast<T*>(shmem.ptr() + sizeof(Header))),
		last(first + (shmem.size() - sizeof(Header)) / sizeof(T))
	{}

	void push(const T& val) {
		first[header->tail++] = val;
	}

	void pop() { header->head++; }
	T& front() { return first[header->head]; }
	const T& front() const { return first[header->head]; }
	uint32_t size() const { return header->tail - header->head; }
	bool empty() const { return size() == 0; }
	Mutex& mutex() { return header->mutex; }
	
private:

	friend std::ostream& operator<<(std::ostream& out, const Shmem_queue& rhs) {
		out << "shmem = " << rhs.shmem << ", head = ";
		if (rhs.header) {
			out << rhs.header->head;
		} else {
			out << "none";
		}
		return out;
	}

	Shared_memory shmem;
	Header* header;
	T* first;
	T* last;

};

template<class T>
struct Shmem_server {

	typedef typename Shmem_queue<T>::Mutex Mutex;
	typedef typename Shmem_queue<T>::Cond Cond;

	explicit Shmem_server(int id):
		_pool(id), _semaphore(gen_name(id)) {}

	Shmem_server(int id, size_t size):
		_pool(id, size), _semaphore(gen_name(id)),
		worker([this] () { serve(); }) {}

	void send(T val) {
		std::lock_guard<Mutex> lock(_pool.mutex());
		_pool.push(val);
		_semaphore.notify_one();
		std::cout << "Send " << val << std::endl;
//		if (val == 'q') {
//			stopped = true;
//		}
	}

	void wait() {
		if (worker.joinable()) {
			worker.join();
		}
	}

private:

	static std::string gen_name(int id) {
		std::stringstream s;
		s << "factory-" << id;
		return s.str();
	}

	void serve() {
		while (!stopped) {
			std::cout << "QQ " << std::endl;
			while (!(stopped || !_pool.empty())) {
				_semaphore.wait();
			}
			std::lock_guard<Mutex> lock(_pool.mutex());
			std::cout << "QQ1" << std::endl;
			if (stopped) { break; }
			std::cout << "QQ2" << std::endl;
			T val = _pool.front();
			_pool.pop();
			std::cout << "Recv " << val << std::endl;
			if (val == 'q') {
				stopped = true;
			}
		}
		std::cout << "Exiting " << std::endl;
	}
	
	Shmem_queue<T> _pool;
	std::thread worker;
	Semaphore _semaphore;
	volatile bool stopped = false;
};


void test_shmem_client() {
	Shmem_server<char> queue(::getppid());
	sleep(2);
	queue.send('a');
	queue.send('q');
	queue.wait();
//	std::cout << "client: queue = " << queue << std::endl;
}

void test_shmem_server(char** argv) {
	Shmem_server<char> queue(::getpid(), 1024);
//	std::cout << "server: queue = " << queue << std::endl;
	Process_group procs;
	procs.add([&argv] () {
		return Process::execute(argv[0], "client");
	});
	queue.wait();
	procs.wait();
}

struct App {
	int run(int argc, char** argv) {
		try {
			if (argc == 1) {
				test_shmem_server(argv);
			} else {
				Command_line cmdline(argc, argv);
				cmdline.parse([&argv] (std::string& arg, std::istream& cmdline) {
					if (arg == "client") test_shmem_client();
					else test_shmem_server(argv);
				});
			}
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};

int main(int argc, char* argv[]) {
	App app;
	return app.run(argc, argv);
}
