#include <factory/factory.hh>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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

template<class T>
struct Shared_memory_queue {

	struct Header {
		uint32_t offset = 0;
		Spin_mutex mutex;
		uint64_t : 56;
	};

	static_assert(sizeof(Header) == 16,
		"Bad Shared_memory_queue header size.");
	
	Shared_memory_queue(int id, size_t max_elems):
		shmem(id, sizeof(T)*max_elems + sizeof(Header))
	{
		std::cout << "sizeof" << sizeof(Header) << std::endl;
		std::cout << "sizeof" << sizeof(std::condition_variable_any) << std::endl;
		header = new (shmem.ptr()) Header;	
		first = new (shmem.ptr() + sizeof(Header)) T[max_elems];
		last = first + max_elems;
	}

	explicit Shared_memory_queue(int id): shmem(id) {
		size_t max_elems = (shmem.size() - sizeof(Header)) / sizeof(T);
		header = reinterpret_cast<Header*>(shmem.ptr());
		first = reinterpret_cast<T*>(shmem.ptr() + sizeof(Header));
		last = first + max_elems;
	}

	void push(const T& val) {
		std::lock_guard<Spin_mutex> lock(header->mutex);
		first[header->offset++] = val;
		cv.notify_one();
	}

	void pop() { header->offset--; }
	T& front() { return first[header->offset]; }
	uint32_t size() const { return header->offset; }
	
private:

	friend std::ostream& operator<<(std::ostream& out, const Shared_memory_queue& rhs) {
		out << "shmem = " << rhs.shmem << ", offset = ";
		if (rhs.header) {
			out << rhs.header->offset;
		} else {
			out << "none";
		}
		return out;
	}

	Shared_memory shmem;
	Header* header;
	T* first;
	T* last;

	std::condition_variable_any cv;
};


void test_shmem_client() {
	Shared_memory_queue<char> queue(::getppid());
	std::cout << "client: queue = " << queue << std::endl;
}

void test_shmem_server(char** argv) {
	Shared_memory_queue<char> queue(::getpid(), 1024);
	std::cout << "server: queue = " << queue << std::endl;
	Process_group procs;
	procs.add([&argv] () {
		return Process::execute(argv[0], "client");
	});
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
