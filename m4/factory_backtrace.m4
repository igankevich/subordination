AC_DEFUN([FACTORY_BACKTRACE], [
	AC_CHECK_HEADERS([execinfo.h])
	AC_CHECK_DECLS([backtrace], [], [], [[#include <execinfo.h>]])
	AC_CHECK_DECLS([backtrace_symbols_fd], [], [], [[#include <execinfo.h>]])
	FACTORY_CXX_FLAG([-rdynamic])
])
