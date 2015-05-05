#include <factory/factory.hh>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "process.hh"

using namespace factory;

struct Shared_memory {

	typedef unsigned char Byte;

	Shared_memory(int id, size_t sz, int perms = DEFAULT_SHMEM_PERMS):
		_addr(nullptr), _id(id), _size(sz), _shmid(0)
	{
		_path = filename(id);
		{ std::ofstream out(_path); }
		::key_t key = check("ftok()", ::ftok(_path.c_str(), PROJ_ID));
		_shmid = check("shmget()", ::shmget(key, size(), perms | IPC_CREAT));
		_addr = static_cast<Byte*>(check("shmat()", ::shmat(_shmid, 0, 0)));
		store_size();
	}

	Shared_memory(int id):
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

	void* ptr() { return _addr; }
	const void* ptr() const { return _addr; }
	size_t size() const { return _size; }
	bool is_owner() const { return _id == ::getpid(); }

private:

	void store_size() {
		Bytes<size_t> raw_size = size();
		std::copy(raw_size.begin(), raw_size.end(), _addr);
	}

	void load_size() {
		Bytes<size_t> raw_size(_addr, _addr + sizeof(size_t));
		_size = raw_size;
	}
	
	std::string filename(int id) const {
		std::stringstream path;
		path << "/var/tmp/factory.";
		path << id;
		path << ".shmem";
		return path.str();
	}

	friend std::ostream& operator<<(std::ostream& out, const Shared_memory& rhs) {
		return out << rhs.ptr()
			<< ", size = " << rhs.size()
			<< ", owner = " << rhs.is_owner()
			<< ", path = " << rhs._path;
	}

	Byte* _addr;
	int _id;
	size_t _size;
	int _shmid;
	std::string _path;

	static const int DEFAULT_SHMEM_PERMS = 0666;
	static const int PROJ_ID = 'Q';
};


void test_shmem_client() {
	Shared_memory mem(::getppid());
	std::cout << "shmaddr client = " << mem << std::endl;
}

void test_shmem_server(char** argv) {
	Shared_memory mem(::getpid(), 1024);
	std::cout << "shmaddr server = " << mem << std::endl;
	Process_group procs;
	procs.add([&argv] () {
		return Process::execute(argv[0], "client");
	});
	procs.wait();
}

struct App {
	int run(int argc, char** argv) {
		try {
			Command_line cmdline(argc, argv);
			cmdline.parse([&argv] (std::string& arg, std::istream& cmdline) {
				if (arg == "client") test_shmem_client();
				else test_shmem_server(argv);
			});
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
