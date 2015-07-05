AC_DEFUN([FACTORY_PTHREAD_AFFINITY], [
	AC_CHECK_DECLS([pthread_setaffinity_np], [], [], [[#include <pthread.h>]])
])
