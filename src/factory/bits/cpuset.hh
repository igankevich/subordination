#ifndef FACTORY_BITS_CPUSET_HH
#define FACTORY_BITS_CPUSET_HH

struct CPU {
	typedef ::cpu_set_t Set;

	CPU(size_t cpu) {
		size_t num_cpus = total_cpus();
		cpuset = CPU_ALLOC(num_cpus);
		_size = CPU_ALLOC_SIZE(num_cpus);
		CPU_ZERO_S(size(), cpuset);
		CPU_SET_S(cpu%num_cpus, size(), cpuset);
	}

	~CPU() { CPU_FREE(cpuset); }

	size_t size() const { return _size; }
	Set* set() { return cpuset; }

private:
	Set* cpuset;
	size_t _size;
};

#endif // FACTORY_BITS_CPUSET_HH
