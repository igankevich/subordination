AC_DEFUN([FACTORY_BACKTRACE], [
	AC_CHECK_HEADERS([execinfo.h])
	AC_CHECK_DECLS([backtrace], [], [], [[#include <execinfo.h>]])
	AC_CHECK_DECLS([backtrace_symbols_fd], [], [], [[#include <execinfo.h>]])
	FACTORY_CXX_FLAG([-rdynamic])
	# the main loop
	AS_VAR_SET([BACKTRACE_LDFLAGS],[""])
	AS_VAR_COPY([save_LDFLAGS],[LDFLAGS])
	for flag in none -lexecinfo; do
		AS_VAR_SET([backtrace_ok],[no])
		AS_VAR_IF([flag],[none],
			[
				AS_VAR_SET([LDFLAGS],[""])
				AC_MSG_CHECKING([whether backtrace works without any flags])
			], [
				AS_VAR_COPY([LDFLAGS],[flag])
				AC_MSG_CHECKING([whether backtrace works with $flag])
			])
		AC_LINK_IFELSE(
			[AC_LANG_PROGRAM(
				[#include <execinfo.h>],
				[[
					void* stack[64];
					int n = backtrace(stack, 64);
					backtrace_symbols_fd(stack, n, 1);
				]]
			)],
			[AS_VAR_SET([backtrace_ok],[yes])],
			[AS_VAR_SET([backtrace_ok],[no])]
		)
		AC_MSG_RESULT([$backtrace_ok])
		AS_VAR_IF([backtrace_ok], [yes], [
			AS_VAR_COPY([BACKTRACE_LDFLAGS],[LDFLAGS])
			break
		],[])
	done
	AS_VAR_COPY([LDFLAGS],[save_LDFLAGS])
	AC_SUBST([BACKTRACE_LDFLAGS])
])
