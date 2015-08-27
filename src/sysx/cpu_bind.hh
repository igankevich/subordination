#ifndef SYSX_CPU_BIND_HH
#define SYSX_CPU_BIND_HH

#include <thread>

#include <sysx/bits/check.hh>

namespace sysx {

	inline size_t
	total_cpus() noexcept {
		return std::thread::hardware_concurrency();
	}

	#if defined(FACTORY_NO_CPU_BINDING)
	void
	thread_affinity(size_t) noexcept {}
	#else
	#if defined(__linux__)
	#include <sched.h>
	#include <sysx/bits/cpuset.hh>
	void
	thread_affinity(size_t c) noexcept {
		CPU cpu(c);
		bits::check(::sched_setaffinity(0, cpu.size(), cpu.set()),
			__FILE__, __LINE__, __func__);
	}
	#elif defined(__sun__)
	#include <sys/processor.h>
	void
	thread_affinity(size_t) noexcept {
		::processor_bind(P_LWPID, P_MYID, cpu_id%total_cpus(), 0);
	}
	#elif defined(__FreeBSD__)
	#include <sys/cpuset.h>
	#include <sysx/bits/cpuset.hh>
	void
	thread_affinity(size_t c) noexcept {
		CPU cpu(c);
		bits::check(::cpuset_setaffinity(CPU_LEVEL_WHICH,
			CPU_WHICH_TID, -1, cpu.size(), cpu.set(),
			__FILE__, __LINE__, __func__));
	}
	#else
	// no cpu binding
	void
	thread_affinity(size_t) noexcept {}
	#endif
	#endif

}

#endif // SYSX_CPU_BIND_HH
