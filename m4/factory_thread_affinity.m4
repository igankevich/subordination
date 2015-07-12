AC_DEFUN([FACTORY_THREAD_AFFINITY], [
	AC_CHECK_DECLS([pthread_setaffinity_np], [], [], [[#include <pthread.h>]])
	AC_CHECK_DECLS([sched_setaffinity], [], [], [[#include <sched.h>]])
	AC_CHECK_HEADERS([sys/cpuset.h sys/processor.h sched.h])
	AC_CHECK_TYPES([cpu_set_t],[],[],[[#include <sched.h>]])
	AC_CHECK_TYPES([cpu_set_t],[],[],[[#include <sys/cpuset.h>]])
])
